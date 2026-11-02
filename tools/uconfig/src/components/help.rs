use crossterm::event::{KeyCode, KeyEvent};
use uconfig_parser::parser::Namespace;
use ratatui::{
    layout::{Constraint, Layout},
    prelude::{Frame, Rect},
    widgets::{
        Block, Clear, Paragraph, Scrollbar, ScrollbarOrientation, ScrollbarState,
        StatefulWidget, Widget, Wrap,
    },
};

use crate::{Component, components::MenuStackAction};


#[derive(Debug, Default)]
pub struct HelpMenu {
    offset: u16,
    content_length: u16,
    viewport: u16,
}

impl HelpMenu {
    pub fn scroll_by(&mut self, delta: i32) {
        let max = self.content_length.saturating_sub(self.viewport);
        self.offset = (self.offset as i32 + delta).clamp(0, max as i32) as u16;
    }
}

const HELP_TEXT: &str = include_str!("../../README.md");

impl Component for HelpMenu {
    fn render(&mut self, frame: &mut Frame, area: Rect, _namespace: &Namespace) {
        let buf = frame.buffer_mut();

        let centered = area.centered(Constraint::Percentage(75), Constraint::Percentage(80));
        Clear.render(centered, buf);

        let block = Block::bordered().title("Help");
        let inner = block.inner(centered);
        block.render(centered, buf);

        let [content, bar] = Layout::horizontal([Constraint::Fill(1), Constraint::Length(1)]).areas(inner);

        let paragraph = Paragraph::new(HELP_TEXT).wrap(Wrap { trim: false });
        self.content_length = paragraph.line_count(content.width.max(1)) as u16;
        self.offset = self.offset.min(self.content_length.saturating_sub(content.height));
        self.viewport = content.height;

        paragraph.scroll((self.offset, 0)).render(content, buf);

        let max_offset = self.content_length.saturating_sub(content.height) as usize;
        let mut scrollbar = ScrollbarState::new(max_offset + 1)
            .position(self.offset as usize)
            .viewport_content_length(content.height as usize);

        Scrollbar::new(ScrollbarOrientation::VerticalRight)
            .begin_symbol(None).end_symbol(None)
            .render(bar, buf, &mut scrollbar);
    }

    fn handle_input(&mut self, _namespace: &mut Namespace, key_event: KeyEvent) -> MenuStackAction {
        match key_event.code {
            KeyCode::Up => self.scroll_by(-1),
            KeyCode::Down => self.scroll_by(1),
            KeyCode::PageUp => self.scroll_by(-20),
            KeyCode::PageDown => self.scroll_by(20),
            KeyCode::Home => self.scroll_by(i32::MIN),
            KeyCode::End => self.scroll_by(i32::MAX),
            KeyCode::Tab | KeyCode::Backspace => return MenuStackAction::Pop,
            _ => {}
        }

        MenuStackAction::None
    }
}
