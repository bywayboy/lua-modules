LUA Modules
=========
作者: 龚辟愚 <http://blog.csdn.net/bywayboy>

C语言 LUA 模块包含如下内容:


*	一个迷你的 LUA模板引擎.
*	一个小型的 JSON 编码/解码器.

##模板引擎用法示例:

以下代码展示了LUA模板引擎的使用方法.
```lua
	local tpl = (require "util").tpl(r.print)		-- r.print is a  writer callback. util.tpl require lmu
	local users={
		{['id']=1,['username']='liigo.zhuang',['age']=26},
		{['id']=2,['username']='bywayboy',['age']=26},
		{['id']=3,['username']='sunwei',['age']=23},
	}
	
	tpl:assign("users", users)
	tpl:assign("title","user list")
	
	tpl:display("./test.html",true) -- compile and show page, cache open.
	
```

###JSON编码解码器用法示例:

```lua
	local lmu = require "lmu"
	
	local users={
		{['id']=1,['username']='liigo.zhuang',['age']=26},
		{['id']=2,['username']='bywayboy',['age']=26},
		{['id']=3,['username']='sunwei',['age']=23},
	}
	
	-- lua table to json string.
	local json_str = lmu.json_encode(users)
	print(json_str)
	
	--- json string to lua table.
	local table = lmu.json_decode(json_str)
	for i,item in pairs(table) do
		print('ID='..item['ID'],'username='..item['username']，'age='..item['age'])
	end
	
	
```