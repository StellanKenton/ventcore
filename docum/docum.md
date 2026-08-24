# 项目开发文档

`docum/` 用于保存跨模块的开发说明、联调步骤和阶段性验收标准。各模块的稳定接口约定仍以对应源码目录内的主文档为准。

| 文档 | 内容 | 依赖代码 |
|---|---|---|
| [`pac-development.md`](pac-development.md) | PAC 模式现状、推荐实现顺序、接口草案与台架验收建议 | `User/app/databus/`、`User/app/ventlogic/`、`User/app/ventalgo/`、`User/bsp/` |

新增本目录文档时，需要同步更新本索引中的内容和依赖关系。
