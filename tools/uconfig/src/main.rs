#![forbid(unsafe_code)]

use std::io;
use uconfig_parser::{config, parser::{self, Namespace}};
use ratatui::DefaultTerminal;

use crate::components::{Component, MainMenu};

pub mod render;
pub mod input;
pub mod components;

#[derive(Debug)]
pub struct App {
    pub namespace: Namespace,
    pub menus: Vec<Box<dyn Component>>,
    pub status: String,
    pub exit: bool,
}

impl Default for App {
    fn default() -> Self {
        Self {
            namespace: Namespace::default(),
            menus: vec![Box::new(MainMenu::default())],
            status: String::default(),
            exit: false,
        }
    }
}

impl App {
    pub fn run(&mut self, namespace: Namespace, terminal: &mut DefaultTerminal) -> io::Result<()> {
        self.namespace = namespace;
        while !self.exit {
            terminal.draw(|frame| self.render(frame))?;
            self.handle_events()?;
        }
        Ok(())
    }
}

fn main() -> io::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let subcommand = args.get(1).map(|s| s.as_str()).unwrap_or("menuconfig");

    let file = "Uconfig";

    let mut namespace = Namespace::default();
    parser::parse(file, &mut namespace);

    if namespace.vector[0].children.is_empty() {
        panic!("Config file has no options to display")
    }

    match subcommand {
        "defconfig" => {
            let _ = std::fs::create_dir_all("include");
            let _ = std::fs::create_dir_all("target");
            std::fs::write("include/config.h", config::export_c_header(&namespace))?;
            std::fs::write(".config", config::export(&namespace))?;
            std::fs::write("target/rust-flags.txt", config::export_rustflags(&namespace))?;
            Ok(())
        }
        "menuconfig" => {
            if let Ok(contents) = std::fs::read_to_string(".config") {
                config::import(&mut namespace, &contents);
            }

            ratatui::run(|terminal| App::default().run(namespace, terminal))
        }
        other => {
            eprintln!("Unknown subcommand: {other}");
            eprintln!("Usage: uconfig [defconfig|menuconfig]");
            std::process::exit(1);
        }
    }
}
