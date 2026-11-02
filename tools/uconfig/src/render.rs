use uconfig_parser::parser::{ConfigOption, Value};
use ratatui::{
    Frame, layout::{Constraint, Direction, Layout}, style::{Color, Stylize}, text::Line, widgets::Block,
};

use crate::{App, components};

pub trait Renderable {
    fn render(&self) -> String;
}

impl Renderable for ConfigOption {
    fn render(&self) -> String {
        let displayname = &self.display;
        let menu_arrow = if self.is_menu { " --->" } else { "" };
        match &self.current {
            Value::Bool(value) => format!("[{}] {displayname} {menu_arrow}", if *value { '*' } else { ' ' }),
            Value::Int(value) => format!("    {displayname} ({value}) {menu_arrow}"),
            Value::String(value) => format!("    {displayname} ({value}) {menu_arrow}"),
            Value::Selection(value) => format!("    {displayname} ({}) {menu_arrow}", value.options[value.selected]),
            Value::Null => format!("    {displayname} {menu_arrow}"),
        }
    }
}

impl App {
    pub fn render(&mut self, frame: &mut Frame) {
        let area = frame.area();
        frame.render_widget(Block::default().bg(Color::Blue), area);

        let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints(vec![
            Constraint::Max(1),  // header
            Constraint::Fill(1), // main content
            Constraint::Max(1),  // status
            Constraint::Max(1),  // footer
        ])
        .split(frame.area());

        frame.render_widget(components::Header, layout[0]);

        for menu in &mut self.menus {
            menu.render(frame, layout[1], &self.namespace);
        }

        if !self.status.is_empty() {
            frame.render_widget(Line::from(self.status.as_str()).centered(), layout[2]);
        }

        frame.render_widget(components::Footer, layout[3]);
    }
}
