# R2.4 ResourceStore

- 已建立 `resource/resource_store.[ch]`，由 store 唯一持有成功打开的 MKF archive 和解压缓存。
- 同一路径重复打开返回同一个 archive；同一 archive/resource index 重复借用返回同一缓存。
- `Rich4ResourceHandle` 是只读 borrow，记录 archive id、resource index 和 store generation；handle 不负责释放，且不得在 store 销毁后使用。
- `rich4_resource_store_destroy` 是唯一归还缓存和 archive 的路径，允许空 store。
- 失败测试覆盖不存在的 archive、越界资源和部分缓存后的统一销毁；正常测试覆盖 archive/cache 去重。
- SDL 运行时的 `open_mkf_from_options` 与 named archive 搜索全部通过 store 打开；`main_sdl.c` 不再直接调用 `rich4_mkf_close`。早退和正常退出都只调用一次 `rich4_resource_store_destroy`。
- `MenuAssets`、`CursorAssets` 和 `GamePreviewAssets` 的长期资源均通过 typed handle 借用。`GamePreviewAssets` 保存 handle 列表；现有 `data/size` 字段仅作为迁移期只读渲染别名，不拥有缓存，也不逐字段释放。
- `free_game_preview_assets` 只释放 handle 列表本身；其后销毁 store，统一释放缓存和 archive。格式探测失败时只丢弃别名，缓存仍由 store 最终回收。
- 临时命令行查看/绘制路径仍可直接读取并在同一函数内释放，不形成长期所有权。
- 验证：`make -B test tools sdl` 全部通过且 `-Wall -Wextra` 无警告；以不存在的数据目录启动，archive 搜索全部失败并经 store 清理后正常报错退出。
- 测试链接：[`tests/resource/test_resource_store.c`](../../tests/resource/test_resource_store.c)。
