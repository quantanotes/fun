use crate::expr::Expr;
use nom::{
    branch::alt,
    bytes::complete::{escaped, is_not, tag, take_while1},
    character::complete::{char, digit1, multispace0, one_of},
    combinator::{cut, map, map_res, recognize},
    error::{context, VerboseError},
    multi::many0,
    sequence::{delimited, preceded, tuple},
    IResult, Parser,
};

pub fn parse(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    preceded(multispace0, alt((parse_atom, parse_list, parse_quote)))(i)
}

pub fn parse_many(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    map(many0(parse), |exprs| match exprs.len() {
        0 => Expr::Void,
        1 => exprs.first().unwrap().to_owned(),
        _ => Expr::List(exprs),
    })(i)
}

fn sexpr<'a, O1, F>(inner: F) -> impl Parser<&'a str, O1, VerboseError<&'a str>>
where
    F: Parser<&'a str, O1, VerboseError<&'a str>>,
{
    delimited(
        preceded(multispace0, char('(')),
        preceded(multispace0, inner),
        context("close paren", cut(preceded(multispace0, char(')')))),
    )
}

fn parse_symbol(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    map(
        recognize(take_while1(move |c| is_symbol_char(c))),
        |s: &str| Expr::Symbol(s.to_string()),
    )(i)
}

fn parse_integer(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    alt((
        map_res(digit1, |s: &str| s.parse::<i64>().map(Expr::Integer)),
        map(preceded(tag("-"), digit1), |s: &str| {
            Expr::Integer(-s.parse::<i64>().unwrap())
        }),
    ))(i)
}

fn parse_float(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    alt((
        map_res(recognize(tuple((digit1, char('.'), digit1))), |s: &str| {
            s.parse::<f64>().map(Expr::Float)
        }),
        map_res(
            preceded(tag("-"), recognize(tuple((digit1, char('.'), digit1)))),
            |s: &str| s.parse::<f64>().map(|x| Expr::Float(-x)),
        ),
    ))(i)
}

fn parse_string(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    delimited(
        char('"'),
        map(
            escaped(is_not(r#""\"#), '\\', one_of(r#""nrt"#)),
            |s: &str| Expr::String(s.to_string()),
        ),
        char('"'),
    )(i)
}

fn parse_atom(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    alt((parse_float, parse_integer, parse_string, parse_symbol))(i)
}

fn parse_list(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    sexpr(many0(parse)).map(Expr::List).parse(i)
}

fn parse_quote(i: &str) -> IResult<&str, Expr, VerboseError<&str>> {
    map(preceded(char('\''), parse), |e| Expr::Quote(Box::new(e)))(i)
}

fn is_symbol_char(c: char) -> bool {
    c.is_alphabetic() || "+-*/=!<>?.:".contains(c)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_symbol() {
        assert_eq!(
            parse_symbol("abc"),
            Ok(("", Expr::Symbol("abc".to_string()))),
            "failed to parse symbol"
        );

        assert_eq!(
            parse_symbol("!symbol?"),
            Ok(("", Expr::Symbol("!symbol?".to_string()))),
            "failed to parse symbol with special characters"
        );
    }

    #[test]
    fn test_parse_integer() {
        assert_eq!(
            parse_integer("3"),
            Ok(("", Expr::Integer(3))),
            "failed to parse positive integer"
        );

        assert_eq!(
            parse_integer("-3"),
            Ok(("", Expr::Integer(-3))),
            "failed to parse negative integer"
        );
    }

    #[test]
    fn test_parse_list() {
        assert_eq!(
            parse_list("()"),
            Ok(("", Expr::List(vec![]))),
            "failed to parse empty list"
        );

        assert_eq!(
            parse_list("(1 2 3)"),
            Ok((
                "",
                Expr::List(vec![Expr::Integer(1), Expr::Integer(2), Expr::Integer(3),])
            )),
            "failed to parse list of integers"
        );

        assert_eq!(
            parse_list("(1 (2 3) 4)"),
            Ok((
                "",
                Expr::List(vec![
                    Expr::Integer(1),
                    Expr::List(vec![Expr::Integer(2), Expr::Integer(3)]),
                    Expr::Integer(4),
                ])
            )),
            "failed to parse nested list"
        );

        assert_eq!(
            parse_list("   (  1   2   3   )"),
            Ok((
                "",
                Expr::List(vec![Expr::Integer(1), Expr::Integer(2), Expr::Integer(3),])
            )),
            "failed to parse list with whitespace"
        );
    }

    #[test]
    fn test_parse() {
        assert_eq!(
            parse("(+ 2 3)"),
            Ok((
                "",
                Expr::List(vec![
                    Expr::Symbol("+".to_string()),
                    Expr::Integer(2),
                    Expr::Integer(3),
                ])
            )),
            "failed to pass simple expression"
        );
    }
}
