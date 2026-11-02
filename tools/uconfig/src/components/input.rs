use crossterm::event::{KeyCode, KeyEvent};
use uconfig_parser::parser::{Namespace, Value};
use ratatui::{
    layout::{Constraint, Layout},
    prelude::{Frame, Line, Rect},
    style::Color,
    widgets::{Block, Clear, Paragraph},
};

use crate::components::Component;

use super::MenuStackAction;

#[derive(Debug)]
pub struct InputMenu {
    index: usize,
    input: String,
    cursor: usize,
    is_str: bool,
    msg: Option<String>,
}

impl InputMenu {
    pub fn new(index: usize, namespace: &Namespace) -> Self {
        let current = &namespace.vector[index].current;

        let input = match current {
            Value::Int(v) => v.to_string(),
            Value::String(v) => v.clone(),
            _ => panic!("attempted to make input menu on invalid type"),
        };

        let is_str = matches!(current, Value::String(_));

        Self {
            index,
            cursor: input.len(),
            input,
            is_str,
            msg: None,
        }
    }

    fn commit(&mut self, namespace: &mut Namespace) -> MenuStackAction {
        if self.is_str {
            namespace.vector[self.index].current = Value::String(self.input.clone());
            return MenuStackAction::Pop;
        }

        match self.input.parse() {
            Ok(v) => namespace.vector[self.index].current = Value::Int(v),
            Err(_) => {
                self.msg = Some(String::from("Input is not a valid number"));
                return MenuStackAction::None;
            }
        }

        MenuStackAction::Pop
    }

    fn insert(&mut self, c: char) {
        self.input.insert(self.cursor, c);
        self.cursor += c.len_utf8();
    }

    fn left(&mut self) {
        self.cursor = self.input.floor_char_boundary(self.cursor.saturating_sub(1));
    }

    fn right(&mut self) {
        self.cursor = self.input.ceil_char_boundary((self.cursor + 1).min(self.input.len()));
    }

    fn backspace(&mut self) {
        let start = self.input.floor_char_boundary(self.cursor.saturating_sub(1));
        self.input.drain(start..self.cursor);
        self.cursor = start;
    }

    fn delete(&mut self) {
        let end = self.input.ceil_char_boundary(self.cursor.saturating_add(1).min(self.input.len()));
        self.input.drain(self.cursor..end);
    }
}

impl Component for InputMenu {
    fn render(&mut self, frame: &mut Frame, area: Rect, namespace: &Namespace) {
        let popup = area.centered(Constraint::Percentage(60), Constraint::Length(7));
        frame.render_widget(Clear, popup);

        let block = Block::bordered().title(format!(" {} ", namespace.vector[self.index].display));
        let inner = block.inner(popup);
        frame.render_widget(block, popup);

        let [hint, field, message] = Layout::vertical([
            Constraint::Length(1),
            Constraint::Length(3),
            Constraint::Length(1),
        ])
        .areas(inner);

        frame.render_widget(
            Paragraph::new(if self.is_str { "Enter text" } else { "Enter a number" }),
            hint,
        );

        let text = Block::bordered().inner(field);
        let cursor_x = Line::from(&self.input[..self.cursor]).width() as u16;
        let offset = cursor_x.saturating_sub(text.width.saturating_sub(1));

        frame.render_widget(
            Paragraph::new(self.input.as_str())
                .block(Block::bordered())
                .scroll((0, offset)),
            field,
        );

        if text.width > 0 {
            frame.set_cursor_position((text.x + (cursor_x - offset), text.y));
        }

        if let Some(msg) = &self.msg {
            frame.render_widget(Paragraph::new(msg.as_str()).style(Color::Red), message);
        }
    }

    fn handle_input(&mut self, namespace: &mut Namespace, key_event: KeyEvent) -> MenuStackAction {
        match key_event.code {
            KeyCode::Esc => return MenuStackAction::Pop,
            KeyCode::Enter => return self.commit(namespace),
            KeyCode::Char(c) => self.insert(c),
            KeyCode::Backspace => self.backspace(),
            KeyCode::Delete => self.delete(),
            KeyCode::Left => self.left(),
            KeyCode::Right => self.right(),
            _ => {}
        }

        self.msg = None;
        MenuStackAction::None
    }
}
