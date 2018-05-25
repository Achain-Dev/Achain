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
   ,PRIMARY KEY (block_num)
-- ,KEY idx_block_num (block_num)
-- ,KEY idx_block_id  (block_id)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COMMENT='块表';

create table transaction_entry_old
(
    trx_id                    varchar(40)                         COMMENT '交易id'
   ,expiration                datetime                            COMMENT '到期时间' 
   ,operations                varchar(256)                        COMMENT '交易中的所有操作'
   ,op_count                  int(2)         UNSIGNED             COMMENT '交易中操作的数量' 
   ,result_trx_type           varchar(40)        NOT NULL         COMMENT '结果交易类型' 
   ,contract_id               varchar(36)                         COMMENT '合约id'
   ,contract_method           varchar(40)                         COMMENT ' 合约方法'
   ,contract_arg              varchar(512)                        COMMENT ' 合约参数'
   ,block_num                 bigint(10)  UNSIGNED    NOT NULL    COMMENT '块号' 
   ,last_update_timestamp     datetime                            COMMENT '最近一次更新的时间' 
   ,PRIMARY KEY (trx_id)
-- ,KEY idx_trx_id (trx_id)
-- ,KEY idx_block_num  (block_num)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COMMENT='交易表';

create table trx_entry
(
    trx_id                      varchar(40)        NOT NULL         COMMENT '交易id'
   ,trx_type                    int(2)         UNSIGNED             COMMENT '交易类型' 
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
   ,PRIMARY KEY (trx_id) 
-- ,KEY idx_trx_id (trx_id)
-- ,KEY idx_block_num  (block_num)
)DEFAULT CHARSET=utf8 COMMENT='交易表';

create table result_to_origin_trx_id
(
    result_trx_id             varchar(40)             NOT NULL   COMMENT '结果交易id' 
   ,origin_trx_id             varchar(40)                        COMMENT '原始交易id'
   ,last_update_timestamp     datetime                           COMMENT '最近一次更新的时间'
    PRIMARY KEY (result_trx_id)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COMMENT='结果交易原始交易id对照表';

create table slot_entry
(
    delegate_id               int(10)                 ‘ NULL   COMMENT '产块代理' 
   ,block_id                  varchar(40)                        COMMENT '块id'
   ,slot_timestamp            datetime                           COMMENT '产块时间'
    PRIMARY KEY (block_id)
)  DEFAULT CHARSET=utf8 COMMENT='结果交易原始交易id对照表';





