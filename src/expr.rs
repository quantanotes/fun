use std::fmt;

use crate::ext::{ExtLambda, ExtMacro};

#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub enum Expr {
    Void,
    Symbol(String),
    Boolean(bool),
    Integer(i64),
    Float(f64),
    String(String),
    List(Vec<Expr>),
    Quote(Box<Expr>),
    Lambda(Vec<String>, Box<Expr>),
    Macro(Vec<String>, Box<Expr>),
    ExtLambda(ExtLambda),
    ExtMacro(ExtMacro),
}

#[derive(Debug, PartialEq)]
pub enum Type {
    Void,
    Symbol,
    Boolean,
    Integer,
    Float,
    String,
    List,
    Quote,
    Lambda,
    Macro,
    Either(Vec<Type>),
}

impl Expr {
    pub fn is(&self, _type: Type) -> bool {
        let expr_type: Type = self.clone().into();
        match _type {
            Type::Either(t) => t.contains(&expr_type),
            _ => expr_type == _type,
        }
    }
}

impl fmt::Display for Expr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Expr::Void => write!(f, "void"),
            Expr::Symbol(e) => write!(f, "{}", e),
            Expr::Boolean(e) => write!(f, "{}", e),
            Expr::Integer(e) => write!(f, "{}", e),
            Expr::Float(e) => write!(f, "{}", e),
            Expr::String(e) => write!(f, r#""{}""#, e),
            Expr::List(e) => {
                write!(f, "(")?;
                for (i, item) in e.iter().enumerate() {
                    if i > 0 {
                        write!(f, " ")?;
                    }
                    write!(f, "{}", item)?;
                }
                write!(f, ")")
            }
            Expr::Quote(e) => write!(f, "'{}", e),
            Expr::Lambda(args, body) => {
                write!(f, "(lambda ({}) {})", args.join(" "), body)
            }
            Expr::Macro(args, body) => {
                write!(f, "(macro ({}) {})", args.join(" "), body)
            }
            Expr::ExtLambda(_) => write!(f, "<external lambda>"),
            Expr::ExtMacro(_) => write!(f, "<external macro>"),
        }
    }
}

impl Into<Type> for Expr {
    fn into(self) -> Type {
        match self {
            Expr::Void => Type::Void,
            Expr::Symbol(_) => Type::Symbol,
            Expr::Boolean(_) => Type::Boolean,
            Expr::Integer(_) => Type::Integer,
            Expr::Float(_) => Type::Float,
            Expr::String(_) => Type::String,
            Expr::List(_) => Type::List,
            Expr::Quote(_) => Type::Quote,
            Expr::Lambda(_, _) => Type::Lambda,
            Expr::Macro(_, _) => Type::Macro,
            Expr::ExtLambda(_) => Type::Lambda,
            Expr::ExtMacro(_) => Type::Macro,
        }
    }
}
