use logos::Logos;

#[derive(Debug, PartialEq, Clone, Default)]
pub enum LexError {
    #[default]
    UnknownToken,
    InvalidInteger,
}

#[derive(Logos, Debug, PartialEq)]
#[logos(error = LexError)]
#[logos(skip r"[ \t\r\n]+")]
#[logos(skip(r"//[^\n]*", allow_greedy = true))]
pub enum Token {
    #[token("proj")]
    Proj,
    #[token("opt")]
    Opt,
    #[token("menu")]
    Menu,
    #[token("import")]
    Import,
    #[token("env")]
    Env,

    #[token("->")]
    Arrow,
    #[token("(")]
    ParenOpen,
    #[token(")")]
    ParenClose,
    #[token("{")]
    BraceOpen,
    #[token("}")]
    BraceClose,
    #[token(":")]
    Colon,
    #[token(",")]
    Comma,

    #[token("display")]
    Display,
    #[token("default")]
    Default,
    #[token("description")]
    Description,
    #[token("depends")]
    Depends,
    #[token("within")]
    Within,

    #[token("select")]
    Select,
    #[token("options")]
    Options,
    #[token("values")]
    Values,

    #[token("string")]
    TypeString,
    #[token("bool")]
    TypeBool,
    #[token("int")]
    TypeInt,
    #[token("null")]
    TypeNull,

    #[regex("true|false", |lex| lex.slice() == "true")]
    Bool(bool),

    #[regex(r"[A-Za-z_][A-Za-z0-9_]*", |lex| lex.slice().to_owned())]
    Identifier(String),

    #[regex(r"[+-]?[0-9]+", |lex| lex.slice().parse::<i64>().map_err(|_| LexError::InvalidInteger))]
    Number(i64),

    #[regex(r#""([^"\\]|\\.)*""#, |lex| {
        let s = lex.slice();
        s[1..s.len() - 1].to_owned()
    })]
    String(String),

    #[regex(r"\[[^\]]*\]", |lex| {
        let s = lex.slice();
        s[1..s.len() - 1].trim().to_owned()
    })]
    Expression(String),
}

#[derive(Logos, Debug, PartialEq)]
#[logos(error = LexError)]
#[logos(skip r"[ \t\r\n]+")]
pub enum ExpressionToken {
    #[token("+")]
    Add,
    #[token("-")]
    Sub,

    #[token("!")]
    Not,
    #[token("&&")]
    And,
    #[token("||")]
    Or,

    #[token("==")]
    Equ,
    #[token("!=")]
    NotEqu,
    #[token(">")]
    Gt,
    #[token("<")]
    Lt,
    #[token(">=")]
    GtEqu,
    #[token("<=")]
    LtEqu,

    #[regex("true|false", |lex| lex.slice() == "true")]
    Boolean(bool),

    #[regex(r"[A-Za-z_][A-Za-z0-9_]*", |lex| lex.slice().to_owned())]
    Identifier(String),

    #[regex(r"[+-]?[0-9]+", |lex| lex.slice().parse::<i64>().map_err(|_| LexError::InvalidInteger))]
    Number(i64),

    #[regex(r#""([^"\\]|\\.)*""#, |lex| {
        let s = lex.slice();
        s[1..s.len() - 1].to_owned()
    })]
    QuotedString(String),
}

#[derive(Logos, Debug, PartialEq)]
#[logos(error = LexError)]
#[logos(skip r"[ \t\r\n]+")]
#[logos(skip(r"#[^\n]*", allow_greedy = true))]
pub enum ConfigToken {
    #[token("=")]
    Equals,

    #[regex("true|false", |lex| lex.slice() == "true")]
    Boolean(bool),

    #[regex(r"[A-Za-z_][A-Za-z0-9_]*", |lex| lex.slice().to_owned())]
    Identifier(String),

    #[regex(r"[+-]?[0-9]+", |lex| lex.slice().parse::<i64>().map_err(|_| LexError::InvalidInteger))]
    Number(i64),

    #[regex(r#""([^"\\]|\\.)*""#, |lex| {
        let s = lex.slice();
        s[1..s.len() - 1].to_owned()
    })]
    QuotedString(String),
}
