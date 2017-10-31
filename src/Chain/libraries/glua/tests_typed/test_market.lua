--[[
合约内所有的alp以及asset都是默认10^5，注意
--ico成功
]]


type Trade = {
    trade_id:string,
    asset:int default 0,
    price:int default 0,
    alp:int default 0,
    timestamp:string,
    user:string,
    operation:int   --0 is asks 1 is bids
}
type User = {
    avi_alp:int default 0,
    avi_asset:int default 0,
    frozen_alp:int default 0,
    frozen_asset:int default 0,
    alp_address:Array<string> default[]
}

type Event = {
    operation:string,
	message:string default '',
	user:User,
	ico_participate:int,
	trade:Trade,
	result:string
}

type Storage = {
    -- insert new storage properties here
    contract_owner:string,
    users:Map<string> default {}, --Map<User>
    --user_alp:Map<int>,
    --user_asset:Map<int>,
    --user_transaction:Map<string>,
    --user_alp_address:Map<string>,
    address_not_partcipate:Map<int> default {},
    asks:Map<string> default {},
    bids:Map<string> default {},
    ICO_participate:Map<int> default {},
    ICO_success:bool default false,
	ICO_begin:bool default false,
    contract_upgraded:bool default false,
    trade_id_trade:Map<string> default {},
	asset_volume:int default 0
}

var M = Contract<Storage>()

function M:init()
    print("初始化",caller_address)
    -- insert init code of contract here
    self.storage.contract_owner=caller_address
    self.storage.users={}
    self.storage.address_not_partcipate={}
    self.storage.asks={}
    self.storage.bids={}
    self.storage.ICO_participate={}
    self.storage.ICO_success=false
    self.storage.ICO_begin=false
    self.storage.contract_upgraded=false
    self.storage.trade_id_trade={}
    self.storage.asset_volume=0
end

let function find_array_string_for_addr(array:Array<string> ,target:string)
    --实行find接口用于Arrray<string>
    for k,value in pairs(array) do
        if value==target then
            return k
        end
    end
    
    return -1
end

let function is_addr_exist(addr:string,users:Map<string>)
    for user,user_info: string in pairs(users) do
        let t_user_info:User = totable(json.loads(user_info))
        var ret=find_array_string_for_addr(t_user_info.alp_address,addr)
        if ret~=-1 then
            var result = [user,ret]
            return result
        end
    end
    
    return nil
    
end

function M:register_contract(info:string)
    --var ret=find_array_string_for_user(self.storage.users,user)
	var json_info=json.loads(info)
	var user:string=tostring(json_info['user'])
    if self.storage.users[user] ~= nil  or (user== 'bonus' )then
    --if ret~=-1 then
        var error_info:Event={}
		error_info.operation='register'
		error_info.message=user.."doesn exist"
        emit error_event(tojsonstring(error_info))
        return
    end
    var n_user:User=User()
    var result:Event={}
    result.operation='register'
	result.user=n_user
    var temp:string = tojsonstring(n_user)
    self.storage.users[user]=temp
    --table.append(self.storage.users, temp)
    emit register_event(tojsonstring(result))
    
end

function M:bind_alp_addr_contract(info:string)
    var json_info=json.loads(info)
    let user:string=tostring(json_info['user'])
    let addr:string=tostring(json_info['addr'])
    var error_info:Event={}
    if self.storage.users[user]==nil then
		error_info.operation='bind'
		error_info.message=user..' doesnt exist'
        emit error_event(tojsonstring(error_info))
        return
    end
    var ret=is_addr_exist(addr,self.storage.users)
    if ret~=nil then
		error_info.operation='bind'
		error_info.message='the addr '..addr..' has exist in anouther user'
        emit error_event(tojsonstring(error_info))
        return
    end
    var t_user:User = totable(json.loads(self.storage.users[user]))
    if self.storage.address_not_partcipate[addr] ~= nil then
        t_user.avi_alp=t_user.avi_alp+self.storage.address_not_partcipate[addr]
        self.storage.address_not_partcipate[addr]=nil
    end
    table.append(t_user.alp_address,addr)
    self.storage.users[user]=tojsonstring(t_user)    
    var result:Event={}
	result.operation='bind'
	result.user=t_user
    emit bind_alp_addr_event(tojsonstring(result))
end

function M:unbind_alp_addr_contract(info:string)
    var json_info=json.loads(info)
    let addr:string=tostring(json_info['addr'])
    let user:string=tostring(json_info['user'])
    var error_info:Event={}

    if self.storage.users[user]==nil then
    --if ret==false then
        error_info.operation='unbind'
		error_info.message=user..' has exist in anouther user'
        emit error_event(tojsonstring(error_info))
        return
    end

    var ret=is_addr_exist(addr,self.storage.users)
    if ret==nil then
        error_info.operation='unbind'
		error_info.message='the addr '..addr..' doesnt exist'
        emit error_event(tojsonstring(error_info))
        return
    elseif tostring(ret[1])~=user then
        error_info.operation='bind'
		error_info.message='the addr '..addr..' has exist in anouther user'
        emit error_event(tojsonstring(error_info))
        return
    end
    
    var t_user_info:User = totable(json.loads(self.storage.users[user])) 
    table.remove(t_user_info.alp_address,ret[2])
    self.storage.users[user]=tojsonstring(t_user_info)
    
    var result:Event={}
	result.operation='unbind'
	result.user=t_user_info
    emit bind_alp_addr_event(tojsonstring(result))
end

function M:on_deposit(num:int)
    var addr:string=caller_address
    var user:string
	var result:Event={}
    var ret=is_addr_exist(addr,self.storage.users)
    if ret~=nil then
        user=tostring(ret[1])
        var t_user_info:User = totable(json.loads(self.storage.users[user]))
        --self.storage.users[user]=tointeger(self.storage.user_alp[user] or 0)+num
        t_user_info.avi_alp=t_user_info.avi_alp+num
        self.storage.users[user]=tojsonstring(t_user_info)
		result.operation='deposit'
		result.user=t_user_info
		
        emit deposit_event(tojsonstring(result))
    else
        self.storage.address_not_partcipate[addr]=tointeger(self.storage.address_not_partcipate[addr] or 0)+num
    end
end

function M:withdraw_contract(info:string)
    var json_info=json.loads(info)
    var user:string=tostring(json_info['user'])
    var amount:int=tointeger(json_info['alp'])
    var addr:string=tostring(json_info['addr'])
    var error_info:Event={}
    
    if self.storage.users[user]==nil then
		error_info.operation='withdraw'
		error_info.message=user..' doesnt exist,pls check'
        emit error_event(tojsonstring(error_info))
        return
    end
    
    var t_user_info:User = totable(json.loads(self.storage.users[user]))
    if table.length(t_user_info.alp_address)==0 then
		error_info.operation='withdraw'
		error_info.message=user ..' doesnt have alp addr'
        emit error_event(tojsonstring(error_info))
        return
    end
    if find_array_string_for_addr(t_user_info.alp_address,addr) ~=-1 then
		error_info.operation='withdraw'
		error_info.message=user..' doesnt have alp addr '..addr
        emit error_event(tojsonstring(error_info))
        return
    end
    
    if (t_user_info.avi_alp < amount) or (t_user_info.avi_alp <= 1000) then
		error_info.operation='withdraw'
		error_info.message=user..' doesnt have enough alp'
        emit error_event(tojsonstring(error_info))
        return
    end
    
    var fee:number=amount*0.001
    if fee<=1000 then
        fee=1000
    end
    
    transfer_from_contract_to_address(addr,'ALP',amount-tointeger(fee))
    
    t_user_info.avi_alp=t_user_info.avi_alp-amount
    self.storage.users[user]=tojsonstring(t_user_info)
    
    var result:Event={}
	result.operation='withdraw'
    result.user=t_user_info	
    emit withdraw_event(tojsonstring(result))
end

function M:start_ICO_contract(info:string)
    if self.storage.ICO_success == true or self.storage.ICO_begin == true then
	    return
	end
	self.storage.ICO_begin=true
    var json_info=json.loads(info)
	var volume:int = tointeger(json_info['volume'])
	self.storage.ICO_participate={}
	self.storage.asset_volume=volume
	
	--emit

end

function M:ICO_participate_contract(info:string)
    if self.storage.ICO_begin == false then
	    return 
	end
    var json_info=json.loads(info)
    var user:string=tostring(json_info['user'])
    var amount:int=tointeger(json_info['alp'])
    var error_info:Event={}
    
    if self.storage.users[user]==nil then
		error_info.operation='ICO_participate'
		error_info.message=user..' doesnt exist'
        emit error_event(tojsonstring(error_info))
        return
    end
    
    var t_user_info:User = totable(json.loads(self.storage.users[user]))
    if t_user_info.avi_alp<amount then
		error_info.operation='ICO_participate'
		error_info.message=user..' doesnt have enough alp'
        emit error_event(tojsonstring(error_info))
        return
    end
    
    t_user_info.avi_alp=t_user_info.avi_alp-amount
    self.storage.users[user]=tojsonstring(t_user_info)
    self.storage.ICO_participate[user]= tointeger(self.storage.ICO_participate[user] or 0) +amount
    var result:Event={}
	result.operation='ICO_participate'
	result.user=t_user_info
	result.ico_participate=self.storage.ICO_participate[user]
    emit ICO_participate_info_envent(tojsonstring(result)) 
end

let function sum(ico_info:Map<int>)
    var sum:int=0
    for k,v in pairs(ico_info) do
	    sum=sum+tointeger(v)
	end
	return sum
end 


function M:distribute_asset_contract()
    if self.storage.ICO_begin == false then
	    return 
	end
    var user:string =''
	var amount:int=0
	var volume=self.storage.asset_volume
	var t_sum=sum(self.storage.ICO_participate)
	var exchange:number = volume/t_sum
    for user,amount in pairs(self.storage.ICO_participate) do
	    
        var t_user_info:User = totable(json.loads(self.storage.users[user]))
        t_user_info.avi_asset=t_user_info.avi_asset+tointeger(amount*exchange)
		self.storage.users[user]=tojsonstring(t_user_info)
        var result:Event={}
		result.operation='distribute_asset'
		result.user=t_user_info
        emit distribute_asset_event(tojsonstring(result)) 
    end
end

function M:set_ICO_flag_contract(info:string)
    if self.storage.ICO_begin == false then
	    return 
	end
    var json_info=json.loads(info)
    var flag:string=tostring(json_info['flag'])
    --false or true
    --ico 成功还是失败
    if flag=='true' then
        self.storage.ICO_success=true
    else
        self.storage.ICO_success=false
    end
end

function M:on_upgrade()
   --判断ico是否成功
   if self.storage.ICO_success== true then
       var result:Event={}
	   result.operation='upgrade'
	   result.message='ICO is suscessfull'
       emit ICO_success_event(tojsonstring(result))
       return
   end
end

function M:ICO_withdraw_contract(info:string)
    --ico资金取出，仅允许合约创建者在ico成功时操作
    if self.storage.ICO_begin == false then
	    return 
	end
    var error_info:Event={}
    if self.storage.ICO_success ==false then
		error_info.operation='ICO_withdraw'
		error_info.message='ico has failed'
        emit error_event(tojsonstring(error_info))
        return
    end
    var sum:int=0
	var k:string=''
    for k,v in pairs(self.storage.ICO_participate) do
        sum=sum+self.storage.ICO_participate[k]
    end 
    
    transfer_from_contract_to_address(self.storage.contract_owner,'ALP',sum)
    
    var result:Event={}
	result.operation='ICO_withdraw'
	result.message='alp has withdrawed'
    emit ICO_withdraw_event(tojsonstring(result))
    
end


function M:on_destroy()
    --若ico失败，则ico资金退回
	var k:string=''
	var v:int =0
    for k,v in pairs(self.storage.ICO_participate) do
        var t_user_info:User = totable(json.loads(self.storage.users[k]))
        var addr:string = t_user_info.alp_address[1]
        transfer_from_contract_to_address(addr,'ALP',v)
    end
    self.storage.ICO_participate={}
    for k,v in pairs(self.storage.address_not_partcipate) do
        transfer_from_contract_to_address(k,'ALP',v)
    end
    self.storage.address_not_partcipate={}
end

let function get_top_map(map:Map<string>)
    var top:Array<object> =nil
    
    for k,v in pairs(map) do
        top=[k,v]
    end
    return top
end

let function get_bottom_map(map:Map<string>)	
	var top:Array<object> =nil
    for k,v in pairs(map) do
        top= [k,v]
		break
    end
	return top
end

let function get_first_array(array:Array<Trade>)
    if table.length(array)==0 then
        return nil
    end
    for k,v in pairs(array) do
        return [k,v]
    end
end


let function modify_users(trade:Trade,users:Map<string>,asset:int,alp:int)
    --修改users用户下的asset and alp 
    if asset ==0 and alp ==0 then 
        return
    end
    var user:string = trade.user
    var t_user_info:User=totable(json.loads(users[user]))
    if trade.operation == 0 then
        t_user_info.frozen_asset=t_user_info.frozen_asset-asset
        t_user_info.avi_alp=t_user_info.avi_alp+alp
    else
        t_user_info.frozen_alp=t_user_info.frozen_alp-alp
        t_user_info.avi_asset=t_user_info.avi_asset+asset
    end
    users[user]=tojsonstring(t_user_info)
    
    var result:Event={}
	result.operation='modify_users'
	result.user=t_user_info
    emit change_event_user(tojsonstring(result))
    
end

let function match(trade:Trade,users:Map<string>,bids:Map<string>,asks:Map<string>,trade_id_trade:Map<string>)
    --挂买卖单之后进行撮合，抛出撮合之后event结果作为最终成交记录
    var t_asset_alp:Array<int> = [trade.asset,trade.alp]
    var temp:Map<string>
    var result:Event ={}
    local map:object 
	local trade_array:object
	local t_trade_array:Array<Trade>
	local asset_asks:int
	local t_trade_record:object
	local temp_asset:int
	local temp_trade:Trade
	local temp_price:int
	local asset_bids:int 
    if trade.operation==0 then
	    map=totable(get_top_map(bids))
		if map ==nil then
		    --asks[tostring(trade.price)]=tojsonstring(trade)
		    return
		end
        trade_array=json.loads(tojsonstring(map[2]))
        if trade_array == nil then
            return
        end
        if trade.price>tointeger(map[1]) then
            return
        end
        t_trade_array = totable(json.loads(tojsonstring(trade_array)))
        repeat
            asset_asks=trade.asset
            t_trade_record=get_first_array(t_trade_array)
            temp_asset=0
            if t_trade_record ~= nil then
			    temp_trade=totable(t_trade_record[2])
				temp_price =temp_trade.price
                asset_bids=tointeger(temp_trade.alp/temp_price)
                if asset_asks>=asset_bids then
                    --asset_asks=asset_asks-asset_bids
                    trade.asset=trade.asset-asset_bids
                    trade.alp=trade.alp+tointeger(asset_bids/trade.price)
                    temp_trade.asset=temp_trade.asset+asset_bids
                    temp_trade.alp=0
                    table.remove(t_trade_array,t_trade_record[1])
                    trade_id_trade[temp_trade.trade_id]=nil
                    if table.length(t_trade_array) ==0 then
                        bids[map[1] ] =nil
                    else
                        bids[map[1] ]=tojsonstring(t_trade_array)
                    end
                    temp_asset=asset_bids
                    result={}
					result.operation='match'
					result.trade=trade
					emit match_event(tojsonstring(result))
                else
                    trade.alp=trade.alp+tointeger(trade.asset/trade.price)
                    trade.asset=0
                    temp_trade.asset=temp_trade.asset+asset_asks
                    temp_trade.alp=temp_trade.alp-tointeger(asset_asks/temp_trade.price)
                    t_trade_array[1]=temp_trade
                    bids[map[1] ]=tojsonstring(t_trade_array)
                    temp_asset=asset_asks
                    result={}
					result.operation='match'
					result.trade=trade
					emit match_event(tojsonstring(result))
                end
				pprint("modify the user",temp_trade,temp_asset)
                modify_users(temp_trade,users,temp_asset,tointeger(temp_asset*temp_trade.price))
            end
			map=totable(get_top_map(bids))
			if map== nil then
			   break
			end
			trade_array=json.loads(tojsonstring(map[2]))
            --t_trade_array=totable(json.loads(get_top_map(bids)))
			t_trade_array = totable(json.loads(tojsonstring(trade_array)))
        until (trade.asset==0) or (trade.price>tointeger(map[1]))
        
    else
        map= totable(get_bottom_map(asks))
		if map== nil then
		    --bids[tostring(trade.price)]=tojsonstring
		    return 
		end
        trade_array=json.loads(tojsonstring(map[2]))
        if trade_array == nil then
            return
        end
		
        if trade.price<tointeger(map[1]) then
            return
        end
		t_trade_array = totable(json.loads(tojsonstring(trade_array)))
        repeat
            asset_bids=tointeger(trade.alp/trade.price)
            t_trade_record=get_first_array(t_trade_array)
			temp_trade=totable(t_trade_record[2])
			temp_price =temp_trade.price
            temp_asset=0
            if t_trade_record ~= nil then
                asset_asks=tointeger(temp_trade.asset)
                if asset_bids>=asset_asks then
                    trade.asset=trade.asset+asset_asks
                    trade.alp=trade.alp-tointeger(asset_asks/trade.price)
                    temp_trade.asset=0
                    temp_trade.alp=temp_trade.alp+tointeger(asset_asks/temp_trade.price)
                    table.remove(t_trade_array,t_trade_record[1])
                    trade_id_trade[temp_trade.trade_id]=nil
                    if table.length(t_trade_array) ==0 then
                        asks[map[1] ] =nil
                    else
                        asks[map[1] ]=tojsonstring(t_trade_array)
                    end
                    temp_asset=asset_asks
                    result={}
					result.operation='match'
					result.trade=trade
					emit match_event(tojsonstring(result))
                else
                    trade.asset=trade.asset+asset_bids
                    trade.alp=0
                    temp_trade.asset=temp_trade.asset-asset_bids
                    temp_trade.alp=temp_trade.alp+tointeger(asset_bids/temp_trade.price)
                    t_trade_array[1]=temp_trade
                    asks[map[1] ]=tojsonstring(t_trade_array)
                    temp_asset=asset_bids
                    result={}
					result.operation='match'
					result.trade=trade
					--emit match_event(tojsonstring(result))
                end
                modify_users(temp_trade,users,temp_asset,tointeger(temp_asset*temp_trade.price))
            end
            map=totable(get_bottom_map(asks))
			if map==nil then
			    break
			end
			trade_array=json.loads(tojsonstring(map[2]))
			t_trade_array = totable(json.loads(tojsonstring(trade_array)))
        until trade.alp==0 or trade.price<tointeger(map[1])
        
    end
    modify_users(trade,users,tointeger(math.abs(trade.asset-t_asset_alp[1] )),tointeger(math.abs(trade.alp-t_asset_alp[2] )))
    if result~={} then
        emit match_event(tojsonstring(result))
    end
end

function M:sell_contract(info:string)
    --sell卖出asset
    --生成trade_id
	if self.storage.ICO_begin == false or self.storage.ICO_success == false then
	    return 
	end
    var json_info=json.loads(info)
    var trade_id:string=tostring(json_info['trade_id'])
    var user:string=tostring(json_info['user'])
    var timestamp:string=tostring(json_info['timestamp'])
    var amount:int=tointeger(json_info['amount'])
    var price:int=tointeger(json_info['price'])
	var error_info:Event={}
	
    if self.storage.users[user]==nil then
        error_info.operation='sell'
		error_info.message=user..' doesnt exist'
		emit error_event(tojsonstring(error_info))
        return
    end
	
    var trade:Trade=Trade()
    trade.trade_id=trade_id
    trade.asset=amount
    trade.price=price
    trade.timestamp=timestamp
    trade.user=user
    trade.operation=0
    var t_price:string =tostring(price)
	
    var t_trade_user:Array<Trade>
    --self.asks[t_price]=tojsonstring(trade)
    if self.storage.asks[t_price]==nil then
        t_trade_user=[]
    else
        t_trade_user=totable(json.loads(self.storage.asks[t_price]))
    end
    
    --self.asks[t_price]=tojsonstring(t_trade_user)
    --修改user下币值
    var t_user_info:User = totable(json.loads(self.storage.users[user]))
	
    t_user_info.avi_asset=t_user_info.avi_asset-amount
    t_user_info.frozen_asset=t_user_info.frozen_asset+amount
    self.storage.users[user]=tojsonstring(t_user_info)
	
    match(trade,self.storage.users,self.storage.bids,self.storage.asks,self.storage.trade_id_trade)
	
    if trade.asset ~=0 then
        table.append(t_trade_user,trade)
		self.storage.asks[t_price]=tojsonstring(t_trade_user)
		var id:string=trade.trade_id
        self.storage.trade_id_trade[id]=tojsonstring(trade)
		var result:Event={}
		result.operation='sell'
		result.user=t_user_info
		result.trade=trade
		emit sell_event(tojsonstring(result))
    end
    
    
end


function M:buy_contract(info:string)
    --buy 买入 asset
    --生成trade_id
	if self.storage.ICO_begin == false or self.storage.ICO_success == false then
	    return 
	end
    var json_info=json.loads(info)
    var trade_id:string=tostring(json_info['trade_id'])
    var user:string=tostring(json_info['user'])
    var timestamp:string=tostring(json_info['timestamp'])
    var alp:int=tointeger(json_info['alp'])
    var price:int=tointeger(json_info['price'])
	var error_info:Event ={}
    if self.storage.users[user]==nil then
        error_info.operation='buy'
		error_info.message=user..' doesnt exist'
		emit error_event(tojsonstring(error_info))
        return
    end
    var trade:Trade=Trade()
    trade.trade_id=trade_id
    trade.alp=alp
    trade.price=price
    trade.timestamp=timestamp
    trade.user=user
    trade.operation=1
    var t_price:string =tostring(price)
    var t_trade_user:Array<Trade>
    --self.asks[t_price]=tojsonstring(trade)
    if self.storage.bids[t_price]==nil then
        t_trade_user=[]
    else
        t_trade_user=totable(json.loads(self.storage.bids[t_price]))
    end
    --修改user下币值
	
    var t_user_info:User = totable(json.loads(self.storage.users[user]))
    t_user_info.avi_alp=t_user_info.avi_alp-alp
    t_user_info.frozen_alp=t_user_info.frozen_alp+alp
    self.storage.users[user]=tojsonstring(t_user_info)
    match(trade,self.storage.users,self.storage.bids,self.storage.asks,self.storage.trade_id_trade)
    --判断是否需要将trade添加至bids or asks队列中
    if trade.alp ~=0 then
        table.append(t_trade_user,trade)
		var id:string=trade.trade_id
        self.storage.trade_id_trade[id]=tojsonstring(trade)
        self.storage.bids[t_price]=tojsonstring(t_trade_user)
		var result:Event={}
		result.operation='buy'
		result.user=t_user_info
		result.trade=trade
		emit buy_event(tojsonstring(result))
    end
    --emit
    --self.bids[t_price]=tojsonstring(t_trade_user)
end

function M:cancel_contract(info:string)
    --针对特定的买卖单(trade_id)进行撤销
	if self.storage.ICO_begin == false or self.storage.ICO_success == false then
	    return 
	end
    var json_info=json.loads(info)
    var user:string = tostring(json_info['user'])
    var trade_id:string= tostring(json_info['trade_id'])
    if self.storage.trade_id_trade[trade_id] == nil then
	    return
	end
	
    var trade:Trade=totable(json.loads(self.storage.trade_id_trade[trade_id]))
    var t_trade_array:Array<Trade>
    var price:string = tostring(trade.price)
    if trade.operation == 0 then
        t_trade_array=totable(json.loads(self.storage.asks[price]))
    else
        t_trade_array=totable(json.loads(self.storage.bids[price]))
    end
    var pos:int=0
    for k: int,v in pairs(t_trade_array) do
	    local t:Trade=totable(v)
        if t.trade_id==trade_id then
            pos=k
        end
    end
    if pos == 0 then
        --emit
        var error_info:Event={}
		error_info.operation='cancel'
		error_info.message=trade_id.. ' doesnt exist'
        emit error_event(tojsonstring(error_info))
        return 
    end
    --remove bids or asks or users
    table.remove(t_trade_array,pos)
    if trade.operation ==0 then
        if table.length(t_trade_array)==0 then
            self.storage.asks[price]=nil
        else
            self.storage.asks[price]=tojsonstring(t_trade_array)
        end
    else
        if table.length(t_trade_array)==0 then
            self.storage.bids[price]=nil
        else
            self.storage.bids[price]=tojsonstring(t_trade_array)
        end
    end
    --恢复users中数据
    user=trade.user
    var t_user_info:User = totable(json.loads(self.storage.users[user]))
    if trade.operation ==0 then
        t_user_info.avi_asset=t_user_info.avi_asset+trade.asset
        t_user_info.frozen_asset=t_user_info.frozen_asset-trade.asset
    else
        t_user_info.avi_alp=t_user_info.avi_alp+trade.alp
        t_user_info.frozen_alp=t_user_info.frozen_alp-trade.alp
    end
    
    self.storage.users[user]=tojsonstring(t_user_info)
    
    
    var result:Event={}
    result.operation='cancel'
	result.trade=trade
	result.user=t_user_info
    emit cancel_event(tojsonstring(result)) 
end

function M:bonus_contract(info:string)
    if self.storage.ICO_begin == false or self.storage.ICO_success == false then
	    return 
	end
    var json_info = json.loads(info)
    --var volume=tointeger(json_info['volume'])
	var user:string =tostring(json_info['user'])
	var amount:int=tointeger(json_info['amount'])
	var volume:int =self.storage.asset_volume
    var result:Event={}
	var error_info:Event={}
    var k:string=''
	var v:string=''
	if caller_address ~= self.storage.contract_owner then
	    error_info.operation='bonus'
		error_info.message=caller_address..' is not owner of contract'
		emit error_event(tojsonstring(error_info))
	    return
	end
	if self.storage.users[user] ==nil then
	    error_info.operation='bonus'
		error_info.message=user..' doesnt exist'
		emit error_event(tojsonstring(error_info))
	    return
	end
	var bonus_user:User=totable(json.loads(self.storage.users[user]))
	if find_array_string_for_addr(bonus_user.alp_address,self.storage.contract_owner) == -1 then
	    error_info.operation='bonus'
		error_info.message=user..' is not ower of contract'
		emit error_event(tojsonstring(error_info))
	    return
	end
	--var bonus_user:Bonus_user=totable(json.loads(self.storage.bonus_user))
	--[[
	判断当前owner地址是否存在
	]]--
	
	if bonus_user.avi_alp<amount then
	    error_info.operation='bonus'
        error_info.message='the avi alp is not enough'		
	    emit error_event(tojsonstring(error_info))
	    return
	end
    for k,v in pairs(self.storage.users) do
        var t_user_info:User = totable(json.loads(v))
        var asset:int=t_user_info.avi_asset+t_user_info.frozen_asset
        var bonus_alp:number=tonumber(asset)/tonumber(volume) * tonumber(amount)
        t_user_info.avi_alp=t_user_info.avi_alp+tointeger(bonus_alp)
        self.storage.users[k]=tojsonstring(t_user_info)
        result={}
		result.operation='bonus'
		result.user=t_user_info
		--result.bonus=bonus_user
        emit bonus_event(tojsonstring(result))
    end
    bonus_user.avi_alp=bonus_user.avi_alp-amount
	self.storage.users[user]=tojsonstring(bonus_user)
    result={}
	result.operation='bonus'
	result.user=bonus_user
    emit bonus_user_bonus_evnet(tojsonstring(result))
end

function M:show_user()
    var k:string=''
	var v:string=''
	var user:User={}
	for k,v in pairs(self.storage.users) do
	    pprint(k)
	    user=totable(json.loads(v))
		pprint(user.avi_alp)
		pprint(user.avi_asset)
		pprint(user.frozen_alp)
		pprint(user.frozen_asset)
		pprint(user.alp_address[1])
	end

end

function M:show_asks_trade()
    var k:string=''
	var v:string = ''
	var value:Trade={}
	var result:Array<object> =[]
	var ret:int
	for k,v in pairs(self.storage.asks) do
	    pprint(k)
		ret=0
		for i,value in pairs(totable(json.loads(v))) do
		    pprint(value.trade_id)
			pprint(value.asset)
			pprint(value.price)
			pprint(value.alp)
			pprint(value.user)
			ret=ret+value.asset
		end
		table.append(result,[tointeger(k),ret])
	end
	pprint('asks queue is ',result)
	var e:Event={}
	e.operation='show_ask_trade'
	e.result=tojsonstring(result)
	emit show_ask_trade_event(tojsonstring(e))
end

function M:show_bids_trade()
    var k:string=''
	var v:string = ''
	var value:Trade={}
	var result:Array<object> =[]
	var ret:int
 	for k,v in pairs(self.storage.bids) do
	    pprint(k)
		ret=0
		for i,value in pairs(totable(json.loads(v))) do
		    pprint(value.trade_id)
			pprint(value.asset)
			pprint(value.price)
			pprint(value.alp)
			pprint(value.user)
			ret=ret+value.alp
		end
		table.insert(result,1,[tointeger(k),tointeger(ret/tointeger(k))])
	end
	var e:Event={}
	pprint('bids queu is ',result)
	e.operation='show_bids_trade'
	e.result=tojsonstring(result)
	emit show_bids_event(tojsonstring(e))
end


function M:get_user_info(info:string)
    var json_info=json.loads(info)
	var user:string=tostring(json_info['user'])
	
	if self.storage.users[user] == nil then
	    return nil
	end
    var t_user_info:User=totable(json.loads(self.storage.users[user]))
	var result:Event={}
	result.operation='get_user_info'
	result.user=t_user_info
	emit user_info_event(tojsonstring(result))
end

function M:test_contract()
    var user1:string = 'hzkai0'
	var user2:string = 'hzkai1'
	var user:User={"avi_alp":1000000,"avi_asset":0,"frozen_alp":0,"alp_address":["ALPxdEoFqVnmobBg69S1rx4sy7PwBViaY6z"],"frozen_asset":0}
	var ico_info:Map<string> ={}
	
	ico_info['volume']='2000000'
	self:start_ICO_contract(tojsonstring(ico_info))
	var reg:Map<string> ={}
	reg['user']=user1
    self:register_contract(tojsonstring(reg))
	reg['user']=user2
	self:register_contract(tojsonstring(reg))
	var user1_bind_alp_address:Map<string> ={}
	var user2_bind_alp_address:Map<string> ={}
	user1_bind_alp_address['user']=user1
	user1_bind_alp_address['addr']="ALPxdEoFqVnmobBg69S1rx4sy7PwBViaY6z"
	user2_bind_alp_address['user']=user2
	user2_bind_alp_address['addr']="ALPHdejoqaqNwLzdN7dwi94eEbPnMKV9rvWV"
	self:bind_alp_addr_contract(json.dumps(user1_bind_alp_address))
	self:bind_alp_addr_contract(json.dumps(user2_bind_alp_address))
	
	pprint("show user 1")
    self:show_user()
	--self:unbind_alp_addr_contract(tojsonstring(user2_bind_alp_address))
	--pprint("show user 2")
	--self:show_user()
	self.storage.users[user1]=tojsonstring(user)
	user={"avi_alp":1000000,"avi_asset":0,"frozen_alp":0,"alp_address":["ALPHdejoqaqNwLzdN7dwi94eEbPnMKV9rvWV"],"frozen_asset":0}
	
	self.storage.users[user2]=tojsonstring(user)
	
	var ico_info_user1:Map<string> ={}
	var ico_info_user2:Map<string> ={}
	ico_info_user1['user']=user1
	ico_info_user2['user']=user2
	ico_info_user1['alp']='100000'
	ico_info_user2['alp']='100000'
	self:ICO_participate_contract(tojsonstring(ico_info_user1))
	self:ICO_participate_contract(tojsonstring(ico_info_user2))
	--self:show_user()
	var set:Map<bool> ={}
	set['flag']=true
	self:set_ICO_flag_contract(tojsonstring(set))
	self:distribute_asset_contract()
	self:show_user()
	--self:show_user()
	
	
	var sell_str:Map<string> = {}
	sell_str['trade_id']='12344555'
	sell_str['user']=user1
	sell_str['time_stamp']='201702091356'
	sell_str['amount']='2000'
	sell_str['price']='3'
	
	self:sell_contract(tojsonstring(sell_str))
	sell_str['trade_id']='12344556'
	sell_str['price']='4'
	self:sell_contract(tojsonstring(sell_str))
	
	self:show_user()
	pprint("sell orders")
	self:show_asks_trade()
	
	var buy_str:Map<string> ={}
	buy_str['trade_id']='12344557'
	buy_str['user']=user2
	buy_str['time_stamp']='201702091528'
	buy_str['alp']='1000'
	buy_str['price']='1'
	self:buy_contract(tojsonstring(buy_str))
	buy_str['time_stamp']='201702191528'
	buy_str['alp']='2000'
	buy_str['price']='2'
	self:buy_contract(tojsonstring(buy_str))
	pprint("users ")
	self:show_user()
	pprint("the sell order is ")
	self:show_asks_trade()
	pprint("the buy order is ")
	self:show_bids_trade()
	
	--self:set_bonus_user_contract()
	
	--[[
	self.storage.bonus_user=tojsonstring(bonus_user)
	var bonus_info:Map<int> ={}
    bonus_info['volume']=1000000*2
    self:bonus_contract()
	self:show_user()
	--]]
end

--[[
var a:table = M 
a.storage = Storage()
a:init()
a:test_contract()
*/
]]

return M




















