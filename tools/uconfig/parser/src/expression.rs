use std::collections::HashSet;

use crate::expression::ExpressionResult::{CircularExpression, Failed, InvalidExpression, Passed};
use crate::lexer::ExpressionToken;
use crate::parser::{BasicValue, ConfigOption, Namespace};
use logos::Logos;

#[derive(Debug, PartialEq, Eq)]
pub enum ExpressionResult {
    Passed,
    Failed,
    InvalidExpression,
    CircularExpression,
}

impl From<ExpressionResult> for bool {
    fn from(result: ExpressionResult) -> Self {
        matches!(result, ExpressionResult::Passed)
    }
}

impl ConfigOption {
    pub fn valid(&self, namespace: &Namespace) -> ExpressionResult {
        if let Some(ref dep) = self.depends {
            return evaluate(namespace, &self.name, dep, &mut HashSet::default());
        }
        ExpressionResult::Passed
    }

    fn valid_internal(&self, namespace: &Namespace, evaluated: &mut HashSet<String>) -> ExpressionResult {
        if let Some(ref dep) = self.depends {
            return evaluate(namespace, &self.name, dep, evaluated);
        }
        ExpressionResult::Passed
    }
}

fn evaluate(namespace: &Namespace, identifier: &str, expression: &str, evaluated: &mut HashSet<String>) -> ExpressionResult {
    let mut stack: Vec<BasicValue> = Vec::default();

    if evaluated.contains(identifier) {
        return ExpressionResult::CircularExpression;
    }

    evaluated.insert(identifier.to_string());

    for token in ExpressionToken::lexer(expression) {
        match token {
            Ok(ExpressionToken::Add) | Ok(ExpressionToken::Sub) => {
                if stack.len() < 2 { return InvalidExpression; }

                let rhs = match stack.pop().unwrap() {
                    BasicValue::Int(v) => v,
                    BasicValue::Bool(v) => v as i64,
                    _ => return InvalidExpression,
                };
                let lhs = match stack.pop().unwrap() {
                    BasicValue::Int(v) => v,
                    BasicValue::Bool(v) => v as i64,
                    _ => return InvalidExpression,
                };

                let result = match token {
                    Ok(ExpressionToken::Add) => lhs.checked_add(rhs),
                    Ok(ExpressionToken::Sub) => lhs.checked_sub(rhs),
                    _ => return InvalidExpression,
                };

                let Some(result) = result else {
                    return InvalidExpression;
                };

                stack.push(BasicValue::Int(result));
            }

            Ok(ExpressionToken::And) | Ok(ExpressionToken::Or) => {
                if stack.len() < 2 { return InvalidExpression; }

                let rhs = match stack.pop().unwrap() {
                    BasicValue::Bool(v) => v,
                    BasicValue::Int(v) => v != 0,
                    BasicValue::String(v) => !v.is_empty(),
                };
                let lhs = match stack.pop().unwrap() {
                    BasicValue::Bool(v) => v,
                    BasicValue::Int(v) => v != 0,
                    BasicValue::String(v) => !v.is_empty(),
                };

                stack.push(BasicValue::Bool(match token {
                    Ok(ExpressionToken::And) => lhs && rhs,
                    Ok(ExpressionToken::Or) => lhs || rhs,
                    _ => return InvalidExpression,
                }));
            }

            Ok(ExpressionToken::Equ) | Ok(ExpressionToken::NotEqu) => {
                if stack.len() < 2 { return InvalidExpression; }

                let rhs = stack.pop().unwrap();
                let lhs = stack.pop().unwrap();

                let equal = match (lhs, rhs) {
                    (BasicValue::Bool(a), BasicValue::Bool(b)) => a == b,
                    (BasicValue::Bool(a), BasicValue::Int(b)) => a as i64 == b,
                    (BasicValue::Bool(a), BasicValue::String(b)) => a != b.is_empty(),
                    (BasicValue::Int(a), BasicValue::Int(b)) => a == b,
                    (BasicValue::Int(a), BasicValue::Bool(b)) => a == b as i64,
                    (BasicValue::String(a), BasicValue::String(b)) => a == b,
                    (BasicValue::String(a), BasicValue::Bool(b)) => a.is_empty() != b,
                    (BasicValue::Int(_), BasicValue::String(_)) => return InvalidExpression,
                    (BasicValue::String(_), BasicValue::Int(_)) => return InvalidExpression,
                };

                stack.push(BasicValue::Bool(match token {
                    Ok(ExpressionToken::Equ) => equal,
                    Ok(ExpressionToken::NotEqu) => !equal,
                    _ => return InvalidExpression,
                }));
            }

            Ok(ExpressionToken::Gt) | Ok(ExpressionToken::Lt) | Ok(ExpressionToken::GtEqu) | Ok(ExpressionToken::LtEqu) => {
                if stack.len() < 2 { return InvalidExpression; }

                let rhs = match stack.pop().unwrap() {
                    BasicValue::Int(v) => v,
                    BasicValue::Bool(v) => v as i64,
                    _ => return InvalidExpression,
                };
                let lhs = match stack.pop().unwrap() {
                    BasicValue::Int(v) => v,
                    BasicValue::Bool(v) => v as i64,
                    _ => return InvalidExpression,
                };

                stack.push(BasicValue::Bool(match token {
                    Ok(ExpressionToken::Gt) => lhs > rhs,
                    Ok(ExpressionToken::Lt) => lhs < rhs,
                    Ok(ExpressionToken::GtEqu) => lhs >= rhs,
                    Ok(ExpressionToken::LtEqu) => lhs <= rhs,
                    _ => return InvalidExpression,
                }));
            }

            Ok(ExpressionToken::Not) => {
                if stack.is_empty() { return InvalidExpression; }
                match stack.pop().unwrap() {
                    BasicValue::Int(v) => stack.push(BasicValue::Int(!v)),
                    BasicValue::Bool(v) => stack.push(BasicValue::Bool(!v)),
                    BasicValue::String(v) => stack.push(BasicValue::Bool(v.is_empty())),
                };
            }

            Ok(ExpressionToken::Boolean(val)) => stack.push(BasicValue::Bool(val)),
            Ok(ExpressionToken::Number(val)) => stack.push(BasicValue::Int(val)),
            Ok(ExpressionToken::QuotedString(val)) => stack.push(BasicValue::String(val)),
            Ok(ExpressionToken::Identifier(val)) => {
                let Some(&idx) = namespace.hashmap.get(&val) else {
                    return InvalidExpression
                };

                match namespace.vector[idx].valid_internal(namespace, evaluated) {
                    Passed => stack.push(namespace.vector[idx].current.to_basic()),
                    Failed => stack.push(BasicValue::Bool(false)),
                    InvalidExpression => return InvalidExpression,
                    CircularExpression => return  CircularExpression,
                }
            }
            _ => return InvalidExpression,
        }
    };

    if stack.is_empty() {
        return InvalidExpression;
    }

    evaluated.remove(identifier);

    match stack.pop() {
        Some(BasicValue::Bool(true)) => Passed,
        Some(BasicValue::Bool(false)) => Failed,
        _ => InvalidExpression,
    }
}
