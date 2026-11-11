---
name: 'CLI Game State'
description: 'game_state*.* 编码规则：保持 game_state.cc 框架化，新主题实现拆到独立的 game_state_<主题>.cc'
applyTo: '**/game_state*.*'
---
保持`game_state.cc`简洁和框架化。所有具体实现通过新增独立的`game_state_<主题>.cc`来实现，不往`game_state.cc`里堆。
