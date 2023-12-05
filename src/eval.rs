use crate::{
    env::Env,
    expr::{Expr, Type},
    ext::{ExtLambda, ExtMacro},
};

#[derive(thiserror::Error, Debug)]
pub enum EvalError {
    #[error("unbound symbol {0}")]
    UnboundSymbol(String),

    #[error("invalid type expected {0:?} received {1:?}")]
    InvalidType(Type, Type),

    #[error("args mismatch expected {0} recieved {1}")]
    ArgsMismatch(usize, usize),

    #[error("empty list")]
    EmptyList,
}

pub fn eval(env: &mut Env, expr: Expr) -> Result<Expr, EvalError> {
    match expr {
        Expr::Symbol(e) => eval_symbol(env, e),
        Expr::Boolean(e) => Ok(Expr::Boolean(e)),
        Expr::Integer(e) => Ok(Expr::Integer(e)),
        Expr::Float(e) => Ok(Expr::Float(e)),
        Expr::String(e) => Ok(Expr::String(e)),
        Expr::List(e) => eval_sexpr(env, e),
        Expr::Quote(e) => Ok(*e),
        _ => Ok(Expr::Void),
    }
}

fn eval_symbol(env: &Env, symbol: String) -> Result<Expr, EvalError> {
    env.get(&symbol).ok_or(EvalError::UnboundSymbol(symbol))
}

fn eval_sexpr(env: &mut Env, sexpr: Vec<Expr>) -> Result<Expr, EvalError> {
    match sexpr.split_first() {
        Some((head, tail)) => match eval(env, head.to_owned())? {
            Expr::Lambda(args, body) => eval_lambda(env, (args, body), tail.to_vec()),
            Expr::Macro(args, body) => eval_macro(env, (args, body), tail.to_vec()),
            Expr::ExtLambda(e) => eval_ext_lambda(env, e, tail.to_vec()),
            Expr::ExtMacro(e) => eval_ext_macro(env, e, tail.to_vec()),
            _ => eval_list(env, tail.to_vec()).map(|list| Expr::List([vec![head.to_owned()], list].concat())),
        },
        None => Ok(Expr::Void),
    }
}

fn eval_list(env: &mut Env, sexpr: Vec<Expr>) -> Result<Vec<Expr>, EvalError> {
    sexpr
        .into_iter()
        .map(|e| eval(env, e))
        .collect::<Result<Vec<Expr>, EvalError>>()
}

fn eval_lambda(
    env: &mut Env,
    lambda: (Vec<String>, Box<Expr>),
    args: Vec<Expr>,
) -> Result<Expr, EvalError> {
    if lambda.0.len() != args.len() {
        return Err(EvalError::ArgsMismatch(lambda.0.len(), args.len()));
    }

    let args = eval_list(env, args)?;

    let mut env = env.branch();

    lambda
        .0
        .iter()
        .zip(args)
        .for_each(|(key, value)| env.put(key, value));

    eval(&mut env, *lambda.1)
}

fn eval_macro(
    env: &mut Env,
    macro_: (Vec<String>, Box<Expr>),
    args: Vec<Expr>,
) -> Result<Expr, EvalError> {
    if macro_.0.len() != args.len() {
        return Err(EvalError::ArgsMismatch(macro_.0.len(), args.len()));
    }

    let mut env = env.branch();

    macro_
        .0
        .iter()
        .zip(args)
        .for_each(|(key, value)| env.put(key, value));

    eval(&mut env, *macro_.1)
}

fn eval_ext_lambda(
    env: &mut Env,
    ext_lambda: ExtLambda,
    args: Vec<Expr>,
) -> Result<Expr, EvalError> {
    let args = eval_list(env, args)?;

    ext_lambda.0(env, args)
}

fn eval_ext_macro(
    env: &mut Env,
    ext_macro: ExtMacro,
    args: Vec<Expr>,
) -> Result<Expr, EvalError> {
    ext_macro.0(env, args)
}
