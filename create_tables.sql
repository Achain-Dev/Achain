create table block_entry
(
    block_num                 bigint(10)  UNSIGNED     NOT NULL   COMMENT '块号' 
   ,block_id                  varchar(40)              NOT NULL   COMMENT '块ID' 
   ,pre_block_id              varchar(40)                         COMMENT '上一个块ID'
   ,block_timestamp           datetime                            COMMENT '产块时间' 
   ,num_trxs                  int(4)       UNSIGNED               COMMENT '块中交易数量' 
   ,block_size                bigint(10)    UNSIGNED              COMMENT '块的大小' 
   ,processing_time           bigint(20)     UNSIGNED             COMMENT '处理时间' 
   ,sync_timestamp            datetime                            COMMENT '开始同步时间' 
   ,last_update_timestamp     datetime                            COMMENT '最近一次更新的时间' 
-- ,PRIMARY KEY (block_num)
-- ,KEY idx_block_num (block_num)
-- ,KEY idx_block_id  (block_id)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COMMENT='块表';

create table trx_entry
(
    trx_id                      varchar(40)        NOT NULL         COMMENT '交易id'
-- ,trx_type                    int(2)         UNSIGNED             COMMENT '交易类型' 
   ,trx_type                    varchar(64)                         COMMENT '交易类型' 
   ,deposit_balance_id          varchar(40)                         COMMENT '存入的余额id'
   ,deposit_amount              bigint(10)                          COMMENT '存入数量' 
   ,withdraw_balance_id         varchar(40)                         COMMENT '取出的余额id'
   ,withdraw_amount             bigint(10)                          COMMENT '取出数量' 
   ,balance_diff                bigint(10)                          COMMENT '差额'
   ,alp_account                 varchar(80)                         COMMENT '子账户id'
   ,expiration                  datetime                            COMMENT '到期时间' 
   ,trx_amount                  double                              COMMENT '交易额' 
   ,asset_id                    int(2)         UNSIGNED             COMMENT '资产id' 
   ,required_fee                bigint(10)                          COMMENT '所需费用' 
   ,transaction_fee             bigint(10)                          COMMENT '交易费' 
   ,op_count                    int(2)         UNSIGNED             COMMENT '交易中操作的数量' 
   ,operations                  varchar(256)                        COMMENT '交易中的所有操作'
   ,contract_id                 varchar(40)                         COMMENT '合约id'
   ,contract_method             varchar(40)                         COMMENT '合约方法'
   ,contract_arg                varchar(512)                        COMMENT '合约参数'
   ,event_type                  varchar(64)                         COMMENT '事件类型'
   ,event_arg                   varchar(1024)                        COMMENT '事件参数'
   ,memo_message                varchar(256)                        COMMENT '交易中的所有操作'
   ,block_num                   bigint(10)  UNSIGNED                COMMENT '块号' 
   ,trx_num                     bigint(10)  UNSIGNED                COMMENT '交易序号' 
   ,last_update_timestamp       datetime                            COMMENT '最近一次更新的时间' 
-- ,PRIMARY KEY (trx_id) 
-- ,KEY idx_trx_id (trx_id)
-- ,KEY idx_block_num  (block_num)
)DEFAULT CHARSET=utf8 COMMENT='交易表';

create table result_to_origin_trx_id
(
    result_trx_id             varchar(40)             NOT NULL   COMMENT '结果交易id' 
   ,origin_trx_id             varchar(40)                        COMMENT '原始交易id'
   ,last_update_timestamp     datetime                           COMMENT '最近一次更新的时间'
-- ,PRIMARY KEY (result_trx_id)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COMMENT='结果交易原始交易id对照表';

create table slot_entry
(
    block_id                  varchar(40)                        COMMENT '块id'
   ,delegate_id               int(10)                            COMMENT '产块代理' 
   ,slot_timestamp            datetime                           COMMENT '产块时间'
-- ,PRIMARY KEY (block_id)
)  DEFAULT CHARSET=utf8 COMMENT='产块slot表';

create table account_entry
(
    account_id                 int(10)                            COMMENT '账号id' 
   ,name                       varchar(40)                        COMMENT '账号名'
   ,address                    varchar(40)                        COMMENT '地址'
   ,owner_key                  varchar(64)                        COMMENT '地址'
   ,registration_date          datetime                           COMMENT '产块时间'
   ,last_update                datetime                           COMMENT '产块时间'
   ,votes_for                  bigint(10)                         COMMENT '投票' 
   ,pay_rate                   int(10)                            COMMENT '支付比例' 
   ,pay_balance                bigint(10)                         COMMENT '支付的工资' 
   ,total_paid                 bigint(10)                         COMMENT '总共需要支付的工资' 
   ,blocks_produced            int(10)                            COMMENT '已产块数' 
   ,blocks_missed              int(10)                            COMMENT '错过的块数' 
   ,account_type               varchar(64)                        COMMENT '账户类型'
   ,meta_data                  varchar(64)                        COMMENT 'data'
-- ,PRIMARY KEY (account_id)
)  DEFAULT CHARSET=utf8 COMMENT='账户表';


create table balance_entry
(
    balance_id                 varchar(40)                        COMMENT '余额id'
   ,asset_id                   int(10)                            COMMENT '资产id' 
   ,slate_id                   bigint(10)    UNSIGNED             COMMENT '投票id' 
   ,condition_type             varchar(64)                        COMMENT '条件类型'
   ,balance_types              varchar(64)                        COMMENT '余额类型'
   ,owner                      varchar(64)                        COMMENT '持有者地址'
   ,balance                    bigint(15)                         COMMENT '余额'
   ,deposit_date               datetime                           COMMENT '存款日期'
   ,last_update                datetime                           COMMENT '最新更新'
   ,meta_data                  varchar(64)                        COMMENT 'metadata' 
-- ,PRIMARY KEY (balance_id)
)  DEFAULT CHARSET=utf8 COMMENT='余额表';

create table asset_entry
(
    asset_id                   int(10)                            COMMENT '资产id' 
   ,asset_symbol               varchar(16)                        COMMENT '资产符号'
   ,asset_name                 varchar(64)                        COMMENT '账号名'
   ,description                varchar(256)                       COMMENT '描述'
   ,public_data                varchar(64)                        COMMENT 'public_data'
   ,issuer_account_id          int(10)                            COMMENT '发行账户id'
   ,asset_precision            bigint(10)                         COMMENT '精度'
   ,registration_date          datetime                           COMMENT '注册时间'
   ,last_update                datetime                           COMMENT '最新更新时间'
   ,maximum_share_supply       bigint(10)                         COMMENT '最大供应'
   ,current_share_supply       bigint(10)                         COMMENT '当前供应'
   ,collected_fees             bigint(10)                         COMMENT '手续费'
   ,flags                      int(10)                            COMMENT '标志' 
   ,issuer_permissions         int(10)                            COMMENT '权限' 
   ,PRIMARY KEY (asset_id)
)  DEFAULT CHARSET=utf8 COMMENT='合约表';


create table contract_entry
(
    contract_id                varchar(40)                        COMMENT '合约id'
   ,contract_name              varchar(64)                        COMMENT '合约名' 
   ,level                      int(10)                            COMMENT '合约级别' 
   ,owner                      varchar(64)                        COMMENT '合约拥有者'
   ,description                varchar(256)                       COMMENT '合约描述'
   ,code_abi                   varchar(1024)                      COMMENT '合约接口'
   ,trx_id                     varchar(40)                        COMMENT '交易id'
   ,last_update                datetime                           COMMENT '最近更新'
   ,PRIMARY KEY (contract_id)
)  DEFAULT CHARSET=utf8 COMMENT='合约表';





