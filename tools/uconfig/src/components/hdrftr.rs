use ratatui::{buffer::Buffer, layout::Rect, style::Stylize, text::Line, widgets::Widget};

pub struct Header;
pub struct Footer;

impl Widget for Header {
    fn render(self, area: Rect, buf: &mut Buffer) {
        let line = Line::from(format!("{} {}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"))).centered();
        line.render(area, buf);
    }
}

impl Widget for Footer {
    fn render(self, area: Rect, buf: &mut Buffer) {
        let line = Line::from("[TAB] Help | [UP] / [DOWN] Navigate | [ENTER] / [SPACE] Select | [ESC] / [BACKSPACE] Back | [F9] Save | [F12] Exit").on_black();
        line.render(area, buf)
    }
}
