pub mod main;
pub use main::MainMenu;

pub mod help;
pub use help::HelpMenu;

pub mod input;
pub use input::InputMenu;

pub mod select;
pub use select::SelectMenu;

pub mod hdrftr;
pub use hdrftr::{Footer, Header};

use uconfig_parser::parser::Namespace;
use crossterm::event::KeyEvent;
use ratatui::{Frame, layout::Rect};

pub enum MenuStackAction {
    None,
    Push(Box<dyn Component>),
    Pop
}

pub trait Component: std::fmt::Debug {
    fn render(&mut self, frame: &mut Frame, area: Rect, namespace: &Namespace);
    fn handle_input(&mut self, namespace: &mut Namespace, key_event: KeyEvent) -> MenuStackAction;
}
