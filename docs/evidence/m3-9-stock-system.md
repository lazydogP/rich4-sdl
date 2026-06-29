# M3.9 股票交易、日更与资产估值证据

## 地址与目标模块

- 固定模块为 Rich4.exe V3.11（SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`）。2026-07-17 以 IDA Pro MCP 复核 `0x428D01`、`0x428D2A`、`0x428E23`、`0x428EC5`、`0x42915A`、`0x4291D6`、`0x4294D5`、`0x42AAFF`、`0x41CF67` 和 `0x457DBC`，并与 `../rich4/asm/rich4_ui_stock.asm`、`rich4_stocks.h`、`rich4_all_stocks.c` 交叉确认。
- 目标实现为 `core/stock.[ch]`；`GameState` 保存 12 支股票、4×12 持仓/均价、144 日历史环、公司排名、市场指数及 RNG 恢复状态。`game_dispatch.c` 是真人/AI 的共同交易入口，`turn_rotation.c` 是完整轮次日结入口。

## IDA 结论

- `stock_info` 原结构为 36 bytes；去掉宿主字符串指针后，`GameStockInfo.f4..f32` 保持原偏移字段形状。原持仓项为 8 bytes：dword 数量和 float 均价；当前状态因此补齐 `player_stock_average_cost[4][12]`。
- `0x428D01` 在全局休市标记非零或配置查询返回 1 时关闭市场。重实现把结果建模为显式 `stock_market_closed`，并通过 `Rich4Config` 的起始日期投影到 `stock_calendar_start_*`；交易和日更都只消费这个统一结果。
- `0x428D2A` 普通买入以 `quantity*f20` 计算成本，从玩家 `+32`（bank）扣款，同时从股票 `f8/f10` 扣数量；持仓均价为 `(truncate(old_amount*old_average)+cost)/new_amount`。`0x428E23` 普通卖出以同一现价计算金额，加到 bank，并把数量加回 `f8/f10`；清仓时均价归零。
- `0x457DBC (__CHP)` 临时把 x87 rounding control 设为 truncate-toward-zero，再执行 `frndint`；因此金额转换不是四舍五入。`0x42AAFF` 买入上限取 `min(f10, truncate(bank/f20))`，买卖前都拒绝 `f6 != 0` 的股票。
- `0x4294D5` 维护公司记录 `+24` owner 和 `+28..+31` 的 one-based 排名：交易玩家先移出旧排名，再按持仓降序插入；相同数量时已有排名在前。现实现保存零基 owner/ranking，并在交易、清算变卖和破产清仓时统一刷新。
- `0x42915A` 在 `f8<=1000` 时令 `f10=f8`；否则以 `(rand()%2000+1000)/10000` 取 `f8` 的 10%..29.99%。新游戏使用注入的 MSVCRT 15-bit stream 初始化可交易量并保存推进后的 state。
- `0x4291D6` 休市时不消耗 RNG。开市后先取一次全市场扰动，再对非停牌、非强制股票各取一次个股扰动；`f7` 高半字节强制 `+10%`，其余非零值强制 `-10%`，`f6` 则价格不变。普通趋势叠加 `f24` 波动与市场量，再按独立股票 `f12` 或公司记录 `+36/10000` 做基本面修正，最后限制到 `[-10,+10]`。
- `0x428EC5` 先计算原始新价，再按价格区间使用 `0.01/0.05/0.1/0.5/1.0` 报价单位沿涨跌方向截齐，最后限制到 `1..9999`。每次日更把结果写入 12×144 float 历史的当前列，cursor 模 144 前进，指数为 `truncate(sum(price)*10)`。
- `0x41CF67` 在完整轮次链中调用 `0x4291D6`；当前 `rich4_turn_advance_once_events` 只在 `TURN_EVENT_DAY_ADVANCED` 时执行股票日更，并以 `stock_last_update_day` 防止恢复/重入造成同日二次更新。

## 输入、输出和副作用

- 输入：统一 `BUY_STOCK/SELL_STOCK` 命令、玩家、股票、数量、银行、`f6/f7/f8/f10/f12/f20/f24/f28`、公司 `+36` 值、市场关闭状态和显式 RNG state。
- 输出：银行、数量、均价、`f8/f10`、公司 owner/ranking；日结输出昨日价、现价、趋势、随机量位、历史 cursor、市场指数和推进后的 RNG state。
- 所有交易先写临时 `GameState` 和临时事件队列；金额/数量溢出、钱/持仓/成交量不足及事件容量不足均不提交。真人和 AI 只在 source 权限检查上不同，随后调用同一个 `rich4_stock_apply_command`。
- `GameStockSnapshot` v1 覆盖股票、持仓/均价、历史、公司关联/值/排名、市场关闭、cursor、last day、指数和 RNG；恢复后下一日更新与连续运行逐字段一致。

## 测试链接与置信度

- `tests/unit/test_stock.c` 覆盖零股、钱不足、持仓不足、休市、停牌、成交量、向零截断、均价、公司控股变化、事件容量原子回滚、强制涨跌、价格上下界、144 环回卷、同日一次、固定 RNG、休市不耗 RNG、快照恢复和资产估值。
- `tests/unit/test_turn_rotation.c` 覆盖完整轮次只触发一次日更；`test_game_dispatch` 保持真人/AI 统一命令权限边界；`test_ledger` 覆盖清算/破产后的持仓清理；`test_map_data` 覆盖 52-byte 公司记录 `+36/+48` 解析。
- `test_stock` 另覆盖新局历史初始化：原版启动链在 `0x42915A` 设置可交易量后调用 `0x4291D6`，开市时只把首个当前价写入历史槽 0 并把 cursor 置为 1，未经历的 143 槽保持零；若启动日休市则不写入且 cursor 保持 0。SDL/core 不再用当前价预填整个 144 槽。
- Windows `make -B test`、`make -B sdl test-original-data` 和 WSL `make -B test-sanitize` 全部通过；V3.11 原资源扫描通过，`-Wall -Wextra`、ASan、UBSan 无诊断。
- 当前置信度：`confirmed`（普通市场买卖、日更、历史、公司排名与估值边界）。

## 缺口与冲突

- `0x428D2A/0x428E23` 的 `mode=0` 分支已按原版两个实际入口覆盖：公司发行认购走独立的公司发行交互（公司记录 `+36/+48`、玩家现金），淘汰清理走股票现价回售（恢复 `f8/f10`、所得进入系统账户）。普通股票 UI/AI 仍只提交 mode=1 的统一交易命令；没有为不存在于原版普通窗口的私募通用 API 增加入口。
- 原版 `0x428D01` 的日历关闭路径已由 `stock_calendar_events`、日期计算和 `rich4_stock_market_is_closed` 接入并由 `test_stock` 覆盖；仍未逐资源核对所有日历 Panel 变体，不把该表现边界写成完整 UI 复刻。
- 144 格历史在每日写入后的行为已确认；新游戏首个历史槽与休市启动边界也已按 `0x42915A → 0x4291D6` 闭合。未经历槽仍保持零，AI 的均值/最小值 helper 继续跳过这些空槽，与原版历史扫描的零值过滤一致。
