use crate::{eval::eval, expr::Expr, ext::*, parser::parse_many};
use std::{cell::RefCell, collections::HashMap, fs, rc::Rc};

#[derive(Clone)]
pub struct Env {
    parent: Option<Rc<RefCell<Env>>>,
    objects: HashMap<String, Expr>,
    modules: HashMap<String, Env>,
}

impl Env {
    pub fn new() -> Env {
        Env {
            parent: None,
            objects: HashMap::new(),
            modules: HashMap::new(),
        }
    }

    pub fn branch(&self) -> Env {
        Env {
            parent: Some(Rc::new(RefCell::new(self.clone()))),
            objects: HashMap::new(),
            modules: self.modules.clone(),
        }
    }

    pub fn get(&self, key: &str) -> Option<Expr> {
        if key.contains("::") {
            let mut parts = key.split("::");
            if let Some(first) = parts.next() {
                let rest: String = parts.collect::<Vec<&str>>().join("::");
                return self.modules.get(first)?.get(&rest);
            }
        }

        match self.objects.get(key) {
            Some(value) => Some(value.clone()),
            None => self.parent.as_ref()?.borrow().get(key),
        }
    }

    pub fn put(&mut self, key: &str, value: Expr) {
        self.objects.insert(key.into(), value);
    }

    pub fn delete(&mut self, key: &str) {
        self.objects.remove(key);
    }

    pub fn put_mod(&mut self, key: &str, value: Expr) {
        let mut env = Env::default();

        eval(&mut env, value).unwrap();

        self.modules.insert(key.to_string(), env);
    }

    pub fn use_(&mut self, key: &str) -> Result<(), String> {
        let parts: Vec<&str> = key.split("::").collect();
        let path = parts.join("/") + ".lisp";
        let name = parts.last().unwrap();
        let raw = fs::read_to_string(path).unwrap();
        let mut env = Env::default();

        eval(&mut env, parse_many(&raw).unwrap().1).unwrap();

        self.modules.insert(name.to_string(), env);
        
        Ok(())
    }
}

impl Default for Env {
    fn default() -> Env {
        let mut env = Env::new();

        env.put("true", Expr::Boolean(true));
        env.put("false", Expr::Boolean(false));

        env.put("+", Expr::ExtLambda(ExtLambda(ext_add)));
        env.put("-", Expr::ExtLambda(ExtLambda(ext_sub)));
        env.put("*", Expr::ExtLambda(ExtLambda(ext_mul)));
        env.put("/", Expr::ExtLambda(ExtLambda(ext_div)));

        env.put("=", Expr::ExtLambda(ExtLambda(ext_eq)));
        env.put(">", Expr::ExtLambda(ExtLambda(ext_gt)));
        env.put("<", Expr::ExtLambda(ExtLambda(ext_lt)));
        env.put(">=", Expr::ExtLambda(ExtLambda(ext_gte)));
        env.put("<=", Expr::ExtLambda(ExtLambda(ext_lte)));

        env.put("and", Expr::ExtLambda(ExtLambda(ext_and)));
        env.put("or", Expr::ExtLambda(ExtLambda(ext_or)));
        env.put("not", Expr::ExtLambda(ExtLambda(ext_not)));

        env.put("cond", Expr::ExtMacro(ExtMacro(ext_cond)));
        env.put("if", Expr::ExtMacro(ExtMacro(ext_if)));

        env.put("cons", Expr::ExtLambda(ExtLambda(ext_cons)));
        env.put("car", Expr::ExtLambda(ExtLambda(ext_car)));
        env.put("cdr", Expr::ExtLambda(ExtLambda(ext_cdr)));

        env.put("list", Expr::ExtLambda(ExtLambda(ext_list)));
        env.put("quote", Expr::ExtMacro(ExtMacro(ext_quote)));

        env.put("fun", Expr::ExtMacro(ExtMacro(ext_fun)));
        env.put("macro", Expr::ExtMacro(ExtMacro(ext_macro)));

        env.put("let", Expr::ExtMacro(ExtMacro(ext_let)));
        env.put("set", Expr::ExtMacro(ExtMacro(ext_set)));

        env.put("def", Expr::ExtMacro(ExtMacro(ext_def)));
        env.put("defun", Expr::ExtMacro(ExtMacro(ext_defun)));
        env.put("defmacro", Expr::ExtMacro(ExtMacro(ext_defmacro)));
        env.put("defmod", Expr::ExtMacro(ExtMacro(ext_defmod)));

        env.put("use", Expr::ExtMacro(ExtMacro(ext_use)));

        env.put("println", Expr::ExtLambda(ExtLambda(ext_println)));

        env
    }
}
