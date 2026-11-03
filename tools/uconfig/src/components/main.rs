use crossterm::event::{KeyCode, KeyEvent};
use ratatui::{
    Frame, layout::{Constraint, Direction, Layout, Rect}, style::{Color, Style, Stylize}, widgets::{Block, List, ListItem, ListState, Scrollbar, ScrollbarOrientation, ScrollbarState }
};

use crate::{Component, components::{HelpMenu, InputMenu, MenuStackAction, select::SelectMenu}, render::Renderable};
use uconfig_parser::{expression::ExpressionResult::{CircularExpression, Failed, InvalidExpression, Passed}, parser::{Namespace, Value}};

#[derive(Debug)]
pub struct MainMenu {
    pub liststate: ListState,
    pub submenu_stack: Vec<usize>,
    pub selected_stack: Vec<usize>,
    pub rendered: usize,
}

impl Default for MainMenu {
    fn default() -> Self {
        let mut liststate = ListState::default();
        liststate.select_first();
        Self { liststate, submenu_stack: Vec::new(), selected_stack: Vec::new(), rendered: 0 }
    }
}

impl MainMenu {
    #[inline]
    fn submenu_index(&self) -> usize {
        self.submenu_stack.last().copied().unwrap_or(0)
    }

    fn move_selection(&mut self, namespace: &Namespace, forward: bool) {
        let submenu = &namespace.vector[self.submenu_index()];
        if submenu.children.is_empty() {
            return;
        }

        let current = self.liststate.selected();
        let len = self.rendered;
        let next_index = match current {
            None => 0,
            Some(i) if forward => (i + 1) % len,
            Some(i) => (i + len - 1) % len,
        };

        self.liststate.select(Some(next_index));
    }

    fn interact(&mut self, namespace: &mut Namespace, prefer_menu: bool) -> MenuStackAction {
        let submenu = &namespace.vector[self.submenu_index()];

        let selected_index = match self.liststate.selected() {
            Some(v) => submenu.children.iter().copied()
            .filter(|&child| namespace.vector[child].valid(namespace) != Failed)
            .nth(v),
            None => return MenuStackAction::None,
        };

        let Some(selected_index) = selected_index else {
            return MenuStackAction::None;
        };

        let selected = &mut namespace.vector[selected_index];

        if selected.is_menu && prefer_menu {
            self.submenu_stack.push(selected_index);
            self.selected_stack.push(self.liststate.selected().unwrap_or(0));
            return MenuStackAction::None
        }

        match &mut selected.current {
            Value::Bool(v) => *v = !*v,
            Value::Int(_) | Value::String(_) => {
                return MenuStackAction::Push(Box::new(InputMenu::new(selected_index, namespace)));
            },
            Value::Selection(_) => {
                return MenuStackAction::Push(Box::new(SelectMenu::new(selected_index, namespace)));
            },
            Value::Null => {
                if selected.is_menu {
                    self.submenu_stack.push(selected_index);
                    self.selected_stack.push(self.liststate.selected().unwrap_or(0));
                    return MenuStackAction::None
                }
            }
        }

        MenuStackAction::None
    }

    fn exit_submenu(&mut self) {
        if self.submenu_stack.pop().is_some() {
            self.liststate.select(self.selected_stack.pop());
        }
    }
}

impl Component for MainMenu  {
    fn render(&mut self, frame: &mut Frame, area: Rect, namespace: &Namespace)  {
        let [_, content, _] = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(5), Constraint::Fill(1), Constraint::Percentage(5)])
            .areas(area);

        let submenu = &namespace.vector[self.submenu_index()];

        let title = match self.submenu_index() {
            0 => submenu.display.clone(),
            _ => format!("{} - {}", namespace.vector[0].display, submenu.display)
        };

        let block = Block::bordered()
            .title(title)
            .bg(Color::Gray)
            .fg(Color::Black);

        let inner = block.inner(content);
        frame.render_widget(block, content);

        let list_items: Vec<ListItem> = submenu.children
        .iter().map(|&child| &namespace.vector[child])
        .filter_map(|option| {
            let text = match option.valid(namespace) {
                Passed => option.render(),
                Failed => return None,
                InvalidExpression => format!("ERR {} HAS AN INVALID EXPRESSION", option.name),
                CircularExpression => format!("ERR {} HAS A CIRCULAR EXPRESSION", option.name)
            };
            Some(ListItem::new(text))
        }) .collect();

        self.rendered = list_items.len();
        let has_scrollbar = self.rendered > inner.height as usize;

        let [list_area, scrollbar_area] = Layout::horizontal([
            Constraint::Fill(1),
            Constraint::Length(if has_scrollbar { 1 } else { 0 }),
        ]).areas(inner);

        let list = List::new(list_items)
            .highlight_style(Style::new().reversed().blue())
            .highlight_symbol(">> ")
            .repeat_highlight_symbol(true);

        frame.render_stateful_widget(list, list_area, &mut self.liststate);

        if has_scrollbar {
            let max_offset = self.rendered - list_area.height as usize;
            let mut scrollbar_state = ScrollbarState::new(max_offset + 1)
                .position(self.liststate.offset().min(max_offset))
                .viewport_content_length(list_area.height as usize);

            frame.render_stateful_widget(
                Scrollbar::new(ScrollbarOrientation::VerticalRight)
                .begin_symbol(None).end_symbol(None)
                .style(Style::new().fg(Color::Black).bg(Color::Gray)),
                scrollbar_area,
                &mut scrollbar_state,
            );
        }
    }

    fn handle_input(&mut self, namespace: &mut Namespace, key_event: KeyEvent) -> MenuStackAction {
        match key_event.code {
            KeyCode::Up => self.move_selection(namespace, false),
            KeyCode::Down => self.move_selection(namespace, true),
            KeyCode::Home => self.liststate.select_first(),
            KeyCode::End => self.liststate.select_last(),
            KeyCode::PageUp => self.liststate.scroll_up_by(10),
            KeyCode::PageDown => self.liststate.scroll_down_by(10),
            KeyCode::Backspace | KeyCode::Esc => self.exit_submenu(),
            KeyCode::Tab => return MenuStackAction::Push(Box::new(HelpMenu::default())),
            KeyCode::Enter => return self.interact(namespace, true),
            KeyCode::Char(' ') => return self.interact(namespace, false),
            _ => {}
        }

        MenuStackAction::None
    }
}
