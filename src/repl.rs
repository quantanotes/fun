use std::{
    borrow::BorrowMut,
    io::{self, Write},
};

use crate::{env::Env, eval::eval, parser::parse};

pub fn repl(verbose: bool) {
    println!("funlisp repl");

    let mut env = Env::default();

    loop {
        print!("funlisp> ");
        io::stdout().flush().unwrap();

        let mut input = String::new();
        io::stdin().read_line(&mut input).expect("io error");

        let input = input.trim();

        if input.to_lowercase() == "exit" {
            println!("the fun is over");
            break;
        }

        match parse(input) {
            Ok((rest, expr)) => {
                if verbose {
                    println!("parser out: {:?}", expr);
                    if !rest.trim().is_empty() {
                        println!("unparsed: {:?}", rest);
                    }
                }

                match eval(env.borrow_mut(), expr) {
                    Ok(result) => println!("{}", result),
                    Err(err) => eprintln!("eval error: {}", err),
                }
            }
            Err(err) => eprintln!("parse error: {:?}", err),
        }
    }
}
