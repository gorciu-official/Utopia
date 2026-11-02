use std::io;

use crossterm::event::{self, Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use uconfig_parser::config;

use crate::{App, components::MenuStackAction};

impl App {
    pub fn handle_events(&mut self) -> io::Result<()> {
        match event::read()? {
            Event::Key(key_event) if key_event.kind == KeyEventKind::Press => self.handle_key_event(key_event),
            _ => {}
        };
        Ok(())
    }

    fn handle_key_event(&mut self, key_event: KeyEvent) {
        self.status.clear();

        match key_event.code {
            KeyCode::Char('c') if key_event.modifiers.contains(KeyModifiers::CONTROL) => {
                self.exit();
                return;
            }
            KeyCode::F(12) => {
                self.exit();
                return;
            }
            KeyCode::F(9) => {
                self.save();
                return;
            }
            _ => {}
        }

        let Some(top) = self.menus.last_mut() else {
            return;
        };

        let action = top.handle_input(&mut self.namespace, key_event);
        match action {
            MenuStackAction::None => {},
            MenuStackAction::Push(component) => { self.menus.push(component); }
            MenuStackAction::Pop => { self.menus.pop(); }
        };
    }

    fn save(&mut self) {
        if let Err(e) = std::fs::write("include/config.h", config::export_c_header(&self.namespace)) {
            self.status = format!("failed to save config header: {e}");
            return;
        }

        if let Err(e) = std::fs::write(".config", config::export(&self.namespace)) {
            self.status = format!("failed to save .config: {e}");
            return;
        }

        self.status = "saved".to_string();
    }

    fn exit(&mut self) {
        self.exit = true;
    }
}
