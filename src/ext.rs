use crate::{
    env::Env,
    eval::{eval, EvalError},
    expr::{Expr, Type},
};
use std::{
    borrow::BorrowMut,
    ops::{Add, Div, Mul, Sub},
};

#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ExtLambda(pub fn(&mut Env, Vec<Expr>) -> Result<Expr, EvalError>);

#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ExtMacro(pub fn(&mut Env, Vec<Expr>) -> Result<Expr, EvalError>);

pub fn ext_add(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    binary_op(expr, 0, 0.0, i64::add, f64::add)
}

pub fn ext_sub(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    binary_op(expr, 0, 0.0, i64::sub, f64::sub)
}

pub fn ext_mul(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    binary_op(expr, 1, 1.0, i64::mul, f64::mul)
}

pub fn ext_div(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    binary_op(expr, 1, 1.0, i64::div, f64::div)
}

pub fn ext_eq(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    Ok(Expr::Boolean(expr.iter().all(|e| e == &expr[0])))
}

pub fn ext_gt(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    comp_op(expr, |x, y| x > y)
}

pub fn ext_lt(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    comp_op(expr, |x, y| x < y)
}

pub fn ext_gte(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    comp_op(expr, |x, y| x >= y)
}

pub fn ext_lte(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    comp_op(expr, |x, y| x <= y)
}

pub fn ext_and(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    Ok(Expr::Boolean(expr.iter().all(|e| match e {
        Expr::Boolean(false) => false,
        _ => true,
    })))
}

pub fn ext_or(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    Ok(Expr::Boolean(expr.iter().any(|e| match e {
        Expr::Boolean(true) => true,
        _ => false,
    })))
}

pub fn ext_not(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 1 {
        return Err(EvalError::ArgsMismatch(1, expr.len()));
    }

    Ok(Expr::Boolean(match &expr[0] {
        Expr::Boolean(b) => !b,
        _ => false,
    }))
}

pub fn ext_cond(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    for expr in expr.iter() {
        match expr {
            Expr::List(conds) => {
                if conds.len() != 2 {
                    return Err(EvalError::ArgsMismatch(2, conds.len()));
                }

                match eval(env, conds[0].to_owned())? {
                    Expr::Boolean(true) => return eval(env, (&conds[1]).to_owned()),
                    Expr::Boolean(false) => continue,
                    _ => return Err(EvalError::InvalidType(Type::List, expr.to_owned().into())),
                };
            }
            _ => return Err(EvalError::InvalidType(Type::List, expr.to_owned().into())),
        };
    }

    Ok(Expr::Void)
}

pub fn ext_if(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 3 {
        return Err(EvalError::ArgsMismatch(3, expr.len()));
    }

    let condition = eval(env, expr[0].to_owned())?;

    match condition {
        Expr::Boolean(true) => eval(env, expr[1].to_owned()),
        Expr::Boolean(false) => eval(env, expr[2].to_owned()),
        _ => Err(EvalError::InvalidType(
            Type::Boolean,
            condition.to_owned().into(),
        )),
    }
}

pub fn ext_cons(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    let head = eval(env, expr[0].to_owned())?;
    let tail = eval(env, expr[1].to_owned())?;

    match tail {
        Expr::List(mut items) => {
            items.insert(0, head);
            Ok(Expr::List(items))
        }
        _ => Err(EvalError::InvalidType(Type::List, tail.to_owned().into())),
    }
}

pub fn ext_car(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 1 {
        return Err(EvalError::ArgsMismatch(1, expr.len()));
    }

    match &expr[0] {
        Expr::List(items) => items.first().cloned().ok_or(EvalError::EmptyList),
        _ => Err(EvalError::InvalidType(
            Type::List,
            expr[0].to_owned().into(),
        )),
    }
}

pub fn ext_cdr(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 1 {
        return Err(EvalError::ArgsMismatch(1, expr.len()));
    }

    match &expr[0] {
        Expr::List(items) => {
            if items.len() > 1 {
                Ok(Expr::List(items[1..].to_vec()))
            } else {
                Ok(Expr::List(Vec::new()))
            }
        }
        _ => Err(EvalError::InvalidType(
            Type::List,
            expr[0].to_owned().into(),
        )),
    }
}

pub fn ext_list(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    Ok(Expr::List(expr))
}

pub fn ext_quote(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 1 {
        return Err(EvalError::ArgsMismatch(1, expr.len()));
    }
    Ok(expr[0].clone())
}

pub fn ext_fun(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    callable(expr).map(|(body, args)| Expr::Lambda(body, args))
}

pub fn ext_macro(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    callable(expr).map(|(body, args)| Expr::Macro(body, args))
}

pub fn ext_let(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    match expr[0].clone() {
        Expr::List(bindings) => {
            let mut local =
                bindings
                    .iter()
                    .try_fold(env.branch(), |mut acc, binding| match binding {
                        Expr::List(pair) if pair.len() == 2 => {
                            let key = match &pair[0] {
                                Expr::Symbol(k) => k.clone(),
                                _ => {
                                    return Err(EvalError::InvalidType(
                                        Type::Symbol,
                                        pair[0].clone().into(),
                                    ))
                                }
                            };
                            let value = eval(env, pair[1].clone())?;
                            acc.put(&key, value);
                            Ok(acc)
                        }
                        _ => Err(EvalError::InvalidType(Type::List, binding.clone().into())),
                    })?;

            eval(local.borrow_mut(), expr[1].clone())
        }
        _ => Err(EvalError::InvalidType(Type::List, expr[0].clone().into())),
    }
}

pub fn ext_set(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    if let (Expr::Symbol(key), value) = (&expr[0], &expr[1]) {
        let value = eval(env, value.to_owned())?;
        env.put(key, value.clone());
        Ok(value)
    } else {
        Err(EvalError::InvalidType(
            Type::Symbol,
            expr[0].to_owned().into(),
        ))
    }
}

pub fn ext_def(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    if let (Expr::Symbol(key), value) = (&expr[0], &expr[1]) {
        let value = eval(env, value.to_owned())?;
        env.put(key, value.clone());
        Ok(value)
    } else {
        Err(EvalError::InvalidType(
            Type::Symbol,
            expr[0].to_owned().into(),
        ))
    }
}

pub fn ext_defun(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    def_callable(expr).map(|(key, body, args)| env.put(&key, Expr::Lambda(body, args)))?;
    Ok(Expr::Void)
}

pub fn ext_defmacro(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    def_callable(expr).map(|(key, body, args)| env.put(&key, Expr::Macro(body, args)))?;
    Ok(Expr::Void)
}

pub fn ext_defmod(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    let key = match expr.first() {
        Some(Expr::Symbol(s)) => s,
        _ => {
            return Err(EvalError::InvalidType(
                Type::Symbol,
                expr[0].to_owned().into(),
            ))
        }
    };

    env.put_mod(&key, expr.last().unwrap().to_owned());

    Ok(Expr::Void)
}

pub fn ext_use(env: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    if expr.len() != 1 {
        return Err(EvalError::ArgsMismatch(1, expr.len()));
    }

    let key = match &expr[0] {
        Expr::Symbol(s) => s,
        _ => {
            return Err(EvalError::InvalidType(
                Type::Symbol,
                expr[0].to_owned().into(),
            ))
        }
    };

    env.use_(&key).unwrap();

    Ok(Expr::Void)
}

pub fn ext_println(_: &mut Env, expr: Vec<Expr>) -> Result<Expr, EvalError> {
    for e in expr {
        print!("{}", e);
    }
    println!();
    Ok(Expr::Void)
}

fn binary_op(
    expr: Vec<Expr>,
    integer_acc: i64,
    float_acc: f64,
    integer_op: fn(i64, i64) -> i64,
    float_op: fn(f64, f64) -> f64,
) -> Result<Expr, EvalError> {
    let first = expr.first().cloned().unwrap_or(Expr::Void);

    match first {
        Expr::Integer(_) => expr
            .iter()
            .try_fold(integer_acc, |acc, x| match x {
                Expr::Integer(e) => Ok(integer_op(acc, *e)),
                _ => Err(EvalError::InvalidType(
                    first.to_owned().into(),
                    x.to_owned().into(),
                )),
            })
            .map(Expr::Integer),
        Expr::Float(_) => expr
            .iter()
            .try_fold(float_acc, |acc, x| match x {
                Expr::Float(e) => Ok(float_op(acc, *e)),
                _ => Err(EvalError::InvalidType(
                    first.to_owned().into(),
                    x.to_owned().into(),
                )),
            })
            .map(Expr::Float),
        _ => Err(EvalError::InvalidType(
            Type::Either(vec![Type::Integer, Type::Float]),
            first.into(),
        )),
    }
}

fn comp_op(expr: Vec<Expr>, op: fn(Expr, Expr) -> bool) -> Result<Expr, EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    match (&expr[0], &expr[1]) {
        (Expr::Integer(_), Expr::Integer(_)) => {
            Ok(Expr::Boolean(op(expr[0].to_owned(), expr[1].to_owned())))
        }
        (Expr::Float(_), Expr::Float(_)) => {
            Ok(Expr::Boolean(op(expr[0].to_owned(), expr[1].to_owned())))
        }
        _ => Err(EvalError::InvalidType(
            Type::Either(vec![Type::Integer, Type::Float]),
            Type::List,
        )),
    }
}

fn callable(expr: Vec<Expr>) -> Result<(Vec<String>, Box<Expr>), EvalError> {
    if expr.len() != 2 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    match expr.as_slice() {
        [Expr::List(args), body] => {
            let args = args
                .iter()
                .filter_map(|arg| match arg {
                    Expr::Symbol(name) => Some(name.clone()),
                    _ => None,
                })
                .collect();
            Ok((args, Box::new(body.to_owned())))
        }
        _ => Err(EvalError::InvalidType(
            Type::List,
            expr[0].to_owned().into(),
        )),
    }
}

fn def_callable(expr: Vec<Expr>) -> Result<(String, Vec<String>, Box<Expr>), EvalError> {
    if expr.len() != 3 {
        return Err(EvalError::ArgsMismatch(2, expr.len()));
    }

    let key = match expr.first() {
        Some(Expr::Symbol(s)) => s,
        _ => {
            return Err(EvalError::InvalidType(
                Type::Symbol,
                expr[0].to_owned().into(),
            ))
        }
    };

    let (args, body) = callable(expr[1..].to_vec())?;

    Ok((key.clone(), args, body))
}
