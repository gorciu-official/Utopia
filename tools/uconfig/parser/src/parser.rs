use crate::lexer::Token;
use logos::Logos;

use std::collections::HashSet;
use std::collections::HashMap;
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BasicValue {
    Bool(bool),
    Int(i64),
    String(String),
}

#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub enum Value {
    #[default]
    Null,
    Bool(bool),
    Int(i64),
    String(String),
    Selection(SelectOption),
}

impl Value {
    pub fn to_basic(&self) -> BasicValue {
        match self {
            Value::Bool(v) => BasicValue::Bool(*v),
            Value::Int(v) => BasicValue::Int(*v),
            Value::String(v) => BasicValue::String(v.clone()),
            Value::Selection(v) => v.values[v.selected].clone(),
            Value::Null => BasicValue::Bool(false)
        }
    }
}

#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub struct SelectOption {
    pub selected: usize,
    pub options: Vec<String>,
    pub values: Vec<BasicValue>,
}

#[derive(Default, Debug)]
pub struct ConfigOption {
    pub name: String,
    pub display: String,
    pub description: Option<String>,
    pub current: Value,
    pub depends: Option<String>,
    pub children: Vec<usize>,

    pub is_menu: bool,
    pub is_env: bool,
    pub mutated: bool,
}

#[derive(Debug)]
pub struct Namespace {
    pub parsed_files: HashSet<PathBuf>,
    pub hashmap: HashMap<String, usize>,
    pub vector: Vec<ConfigOption>,
}

impl Default for Namespace {
    fn default() -> Self {
        let rootnode = ConfigOption { display: "Project".to_string(), ..Default::default()};
        Self {
            parsed_files: HashSet::new(),
            hashmap: HashMap::new(),
            vector: vec![rootnode],
        }
    }
}

struct ErrorInfo<'a> {
    file_path: &'a PathBuf,
    source: &'a String
}

fn parser_error(info: &ErrorInfo, span: std::ops::Range<usize>, message: &str) -> ! {
    let before = &info.source[..span.start];
    let line = before.matches('\n').count() + 1;
    let column = before.rfind('\n').map_or(span.start + 1, |pos| span.start - pos);

    eprintln!("error: {message}");
    eprintln!(" --> {}:{line}:{column}", info.file_path.display());

    if let Some(line_text) = info.source.lines().nth(line - 1) {
        eprintln!("{line:>4} | {line_text}");
        eprintln!("     | {}^", " ".repeat(column - 1));
    }

    std::process::exit(1);
}

macro_rules! parse_string_variant {
    ($name:ident, $variant:ident, $expected:literal) => {
        fn $name(tokens: &mut logos::Lexer<Token>, errinfo: &ErrorInfo) -> String {
            if tokens.next() != Some(Ok(Token::Colon)) {
                parser_error(errinfo, tokens.span(), "expected ':'");
            }
            let Some(Ok(Token::$variant(string))) = tokens.next() else {
                parser_error(errinfo, tokens.span(), $expected);
            };
            string
        }
    };
}

parse_string_variant!(parse_string, String, "expected a string");
parse_string_variant!(parse_expression, Expression, "expected an expression");
parse_string_variant!(parse_identifier, Identifier, "expected an identifier");

fn parse_array<T>( tokens: &mut logos::Lexer<Token>, errinfo: &ErrorInfo, noun: &str, mut convert: impl FnMut(Token) -> Option<T>) -> Vec<T> {
    if tokens.next() != Some(Ok(Token::Colon)) {
        parser_error(errinfo, tokens.span(), "expected ':'");
    }

    if tokens.next() != Some(Ok(Token::BraceOpen)) {
        parser_error(errinfo, tokens.span(), "expected '{'");
    }

    let mut items: Vec<T> = Vec::new();
    let mut previous_was_item = false;

    while let Some(token) = tokens.next() {
        match token {
            Ok(Token::Comma) => {
                if !previous_was_item {
                    parser_error(errinfo, tokens.span(), &format!("expected a {noun}"));
                }
                previous_was_item = false;
            }
            Ok(Token::BraceClose) => {
                if items.is_empty() {
                    parser_error(errinfo, tokens.span(), &format!("{noun} array cannot be empty"));
                }
                if !previous_was_item {
                    parser_error(errinfo, tokens.span(), &format!("expected a {noun}"));
                }
                return items;
            }
            other => match other.ok().and_then(&mut convert) {
                Some(item) if !previous_was_item => {
                    items.push(item);
                    previous_was_item = true;
                }
                _ => {
                    if previous_was_item {
                        parser_error(errinfo, tokens.span(), "expected a comma");
                    } else {
                        parser_error(errinfo, tokens.span(), &format!("expected a {noun}"));
                    }
                }
            },
        }
    }

    parser_error(errinfo, tokens.span(), "expected '}'");
}

fn parse_attribute_default(option: &ConfigOption, tokens: &mut logos::Lexer<Token>, errinfo: &ErrorInfo) -> Value {
    if tokens.next() != Some(Ok(Token::Colon)) {
        parser_error(errinfo, tokens.span(), "expected ':'");
    }

    if matches!(option.current, Value::Null) {
        parser_error(errinfo, tokens.span(), "null option should not have a default");
    }

    match &option.current {
        Value::Bool(_) => {
            let Some(Ok(Token::Bool(val))) = tokens.next() else {
                parser_error(errinfo, tokens.span(), "expected a boolean");
            };
            Value::Bool(val)
        }
        Value::Int(_) => {
            let Some(Ok(Token::Number(val))) = tokens.next() else {
                parser_error(errinfo, tokens.span(), "expected an integer");
            };
            Value::Int(val)
        }
        Value::String(_) | Value::Selection(_) => {
            let Some(Ok(Token::String(val))) = tokens.next() else {
                parser_error(errinfo, tokens.span(), "expected a string");
            };
            Value::String(val)
        }
        Value::Null => unreachable!()
    }
}

fn parse_attributes(namespace: &mut Namespace, option: &mut ConfigOption, tokens: &mut logos::Lexer<Token>, errinfo: &ErrorInfo) {
    let mut display: Option<String> = None;
    let mut default: Option<Value> = None;
    let mut options: Option<Vec<String>> = None;
    let mut values: Option<Vec<BasicValue>> = None;
    let mut parent_idx = 0;

    while let Some(token) = tokens.next() {
        match token {
            Ok(Token::Description) => option.description = Some(parse_string(tokens, errinfo)),
            Ok(Token::Depends) => option.depends = Some(parse_expression(tokens, errinfo)),
            Ok(Token::Display) => display = Some(parse_string(tokens, errinfo)),
            Ok(Token::Default) => default = Some(parse_attribute_default(option, tokens, errinfo)),
            Ok(Token::Within) => {
                let parent = parse_identifier(tokens, errinfo);
                let Some(&idx) = namespace.hashmap.get(&parent) else {
                    parser_error(errinfo, tokens.span(), &format!("unknown identifier '{parent}'"));
                };
                if !namespace.vector[idx].is_menu {
                    parser_error(errinfo, tokens.span(), &format!("'{parent}' is not a menu"));
                }
                parent_idx = idx;
            }
            Ok(Token::Options) => {
                if !matches!(option.current, Value::Selection(_)) {
                    parser_error(errinfo, tokens.span(), "unexpected attribute 'options'");
                }
                options = Some(parse_array(tokens, errinfo, "string", |t| match t {
                    Token::String(s) => Some(s),
                    _ => None,
                }));
            }
            Ok(Token::Values) => {
                if !matches!(option.current, Value::Selection(_)) {
                    parser_error(errinfo, tokens.span(), "unexpected attribute 'values'");
                }
                values = Some(parse_array(tokens, errinfo, "value", |t| match t {
                    Token::String(s) => Some(BasicValue::String(s)),
                    Token::Number(n) => Some(BasicValue::Int(n)),
                    Token::Bool(b) =>   Some(BasicValue::Bool(b)),
                    _ => None,
                }));
            }
            Ok(Token::BraceClose) => break,
            _ => parser_error(errinfo, tokens.span(), "expected an attribute"),
        }
    }

    let Some(display) = display else {
        parser_error(errinfo, tokens.span(), "option is missing a 'display' attribute");
    };
    option.display = display;

    let default = match default {
        Some(default) => default,
        None => {
            if option.current != Value::Null {
                parser_error(errinfo, tokens.span(), "option is missing a 'default' attribute");
            }
            Value::Null
        }
    };

    match option.current {
        Value::Selection(_) => {
            let Some(options) = options else {
                parser_error(errinfo, tokens.span(), "option is missing an 'options' attribute");
            };
            let Some(values) = values else {
                parser_error(errinfo, tokens.span(), "option is missing a 'values' attribute");
            };
            if options.len() != values.len() {
                parser_error(errinfo, tokens.span(), "'options' and 'values' must have the same length");
            }

            let Value::String(default) = default else {
                parser_error(errinfo, tokens.span(), "'default' attribute is not a string");
            };
            let Some(selected) = options.iter().position(|x| x == &default) else {
                parser_error(errinfo, tokens.span(), "'default' attribute is not a given option");
            };

            option.current = Value::Selection(SelectOption { selected, options, values })

        }
        _ => option.current = default,
    }

    let this_idx = namespace.vector.len();
    namespace.vector[parent_idx].children.push(this_idx);
}

fn parse_option(namespace: &mut Namespace, tokens: &mut logos::Lexer<Token>, errinfo: &ErrorInfo) {
    let mut option = ConfigOption::default();

    let Some(Ok(Token::Identifier(ident))) = tokens.next() else {
        parser_error(errinfo, tokens.span(), "expected an identifier");
    };

    option.name = ident.clone();

    if namespace.hashmap.contains_key(&ident) {
        parser_error(errinfo, tokens.span(), "attempted to redefine an option");
    }

    if tokens.next() != Some(Ok(Token::ParenOpen)) {
        parser_error(errinfo, tokens.span(), "expected '('");
    }

    let option_type = tokens.next();

    // set to a blank default value that will get set within parse_attributes
    match option_type {
        Some(Ok(Token::TypeInt)) => option.current = Value::Int(0),
        Some(Ok(Token::TypeBool)) => option.current = Value::Bool(false),
        Some(Ok(Token::TypeNull)) => option.current = Value::Null,
        Some(Ok(Token::TypeString)) => option.current = Value::String(String::default()),
        Some(Ok(Token::Select)) => option.current = Value::Selection(SelectOption::default()),
        _ => parser_error(errinfo, tokens.span(), "expected an option type"),
    };

    if tokens.next() != Some(Ok(Token::ParenClose)) {
        parser_error(errinfo, tokens.span(), "expected ')'");
    }

    let mut got_arrow = false;

    while let Some(token) = tokens.next() {
        match token {
            Ok(Token::BraceOpen) => break,
            Ok(Token::Arrow) => got_arrow = true,
            Ok(Token::Menu) => {
                if ! got_arrow {
                    parser_error(errinfo, tokens.span(), "expected '->'");
                }

                option.is_menu = true;
            }
            _ => parser_error(errinfo, tokens.span(), "expected '{'"),
        }
    }

    parse_attributes(namespace, &mut option, tokens, errinfo);

    let idx = namespace.vector.len();
    namespace.vector.push(option);
    namespace.hashmap.insert(ident, idx);
}

pub fn parse(file: &str, namespace: &mut Namespace) {
    let Ok(path) = std::fs::canonicalize(file) else {
        panic!("Failed to open file {file}");
    };

    if namespace.parsed_files.contains(&path) {
        return;
    }

    namespace.parsed_files.insert(path.clone());

    let source = std::fs::read_to_string(&path).expect("Failed to open file");

    let errinfo = ErrorInfo {
        file_path: &path,
        source: &source,
    };

    let mut tokens = Token::lexer(&source);

    while let Some(token) = tokens.next() {
        match token {
            Ok(Token::Opt) => parse_option(namespace, &mut tokens, &errinfo),
            Ok(Token::Import) => {
                let Some(Ok(Token::String(string))) = tokens.next() else {
                    parser_error(&errinfo, tokens.span(), "expected a string");
                };
                parse(&string, namespace);
            }
            Ok(Token::Proj) => {
                let Some(Ok(Token::String(string))) = tokens.next() else {
                    parser_error(&errinfo, tokens.span(), "expected a string");
                };
                namespace.vector[0].display = string;
            }
            Ok(Token::Env) => {
                let Some(Ok(Token::Identifier(ident))) = tokens.next() else {
                    parser_error(&errinfo, tokens.span(), "expected an identifier");
                };
                match std::env::var(&ident) {
                    Ok(val) => {
                        let option = ConfigOption {
                            current: Value::String(val),
                            is_env: true,
                            ..Default::default()
                        };

                        let idx = namespace.vector.len();
                        namespace.vector.push(option);
                        namespace.hashmap.insert(ident, idx);
                    }
                    Err(_) => parser_error(&errinfo, tokens.span(), "env var is not valid"),
                }
            }
            _ => parser_error(&errinfo, tokens.span(), "expected a keyword"),
        }
    }
}
