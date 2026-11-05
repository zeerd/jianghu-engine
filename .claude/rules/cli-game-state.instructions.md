---
name: 'CLI Game State'
description: 'CLI game state coding'
applyTo: '**/game_state*.*'
---
保持`game_state.cc`简洁和框架化。所有具体实现通过新增独立的`game_state_<主题>.cc`来实现（现存`game_state_info.cc`、`game_state_question.cc`、`game_state_target.cc`），不往`game_state.cc`里堆。
