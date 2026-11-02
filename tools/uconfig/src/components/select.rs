use std::cmp::min;

use crossterm::event::{KeyCode, KeyEvent};
use uconfig_parser::parser::{Namespace, Value};
use ratatui::{layout::Constraint, prelude::Frame, style::Style, widgets::{Block, Clear, List, ListItem, ListState}};

use crate::components::{Component, MenuStackAction};

#[derive(Debug)]
pub struct SelectMenu {
    index: usize,
    max: usize,
    selected: usize,
}

impl SelectMenu {
    pub fn new(index: usize, namespace: &Namespace) -> Self {
        let current = &namespace.vector[index].current;

        match current {
            Value::Selection(v) => Self { index, max: v.options.len() - 1 , selected: v.selected },
            _ => panic!("attempted to make selection menu on invalid type"),
        }
    }

    fn interact(&mut self, namespace: &mut Namespace) -> MenuStackAction {
        let current = &mut namespace.vector[self.index].current;

        let Value::Selection(v) = current else {
            unreachable!()
        };

        v.selected = self.selected;

        MenuStackAction::Pop
    }
}

impl Component for SelectMenu {
    fn render(&mut self, frame: &mut Frame, area: ratatui::prelude::Rect, namespace: &Namespace) {
        let popup = area.centered(Constraint::Percentage(50), Constraint::Percentage(60));
        frame.render_widget(Clear, popup);

        let node = &namespace.vector[self.index];
        let Value::Selection(current) = &node.current else {
            unreachable!()
        };

        let items: Vec<ListItem> = current.options
            .iter()
            .map(|option| ListItem::new(option.clone()))
            .collect();

        let list = List::new(items)
            .block(Block::bordered().title(node.display.clone()))
            .highlight_style(Style::new().reversed())
            .highlight_symbol("> ");

        let mut liststate = ListState::default();
        liststate.select(Some(self.selected));
        frame.render_stateful_widget(list, popup, &mut liststate);
    }

    fn handle_input(&mut self, namespace: &mut Namespace, key_event: KeyEvent) -> MenuStackAction {
        match key_event.code {
            KeyCode::Up => self.selected = self.selected.saturating_sub(1),
            KeyCode::Down => self.selected = min(self.selected + 1, self.max),
            KeyCode::Enter | KeyCode::Char(' ') => return self.interact(namespace),
            KeyCode::Esc | KeyCode::Backspace => return MenuStackAction::Pop,
            _ => {},
        }

        MenuStackAction::None
    }
}
