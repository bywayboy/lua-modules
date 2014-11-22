LUA Modules
=========
作者: 龚辟愚 <http://blog.csdn.net/bywayboy>

这个扩展的设计思路是,一切从简, json 默认支持UTF8. 但没有编码转换, 因此 如果有其他特殊字符，您的LUA源代码必须是UTF8 编码才可以正常工作，其它编码我没有测试过. C语言 LUA 模块包含如下内容:


*	一个迷你的 LUA模板引擎.
*	一个小型的 JSON 编码/解码器.

##编译和安装.

这个模块是基于LUA 5.2 开发的。它运行需要依赖 lua的库. 编译它之前您需要先安装 cmake 编译方法:

```shell
	git clone https://github.com/bywayboy/lua-modules.git
	cd lua-modules
	cmake .
	make && make install
<<<<<<< HEAD
```
=======
``
>>>>>>> 89bb08b332236582209f6f5bf93d7e165eec5899


##模板引擎用法示例:

以下代码展示了LUA模板引擎的使用方法.
```lua
	-- r.print is a  writer callback. util.tpl require lmu
	local tpl = (require "util").tpl(r.print)		
	local users={
		{['ID']=1,['username']='liigo.zhuang',['age']=26},
		{['ID']=2,['username']='bywayboy',['age']=26},
		{['ID']=3,['username']='sunwei',['age']=23},
	}
	
	tpl:assign("users", users)
	tpl:assign("title","user list")
	
	-- compile and show page, cache open.
	tpl:display("./test.html",true) 
	
```

##JSON编码解码器用法示例:

JSON 部分提供2个函数 json_encode 将 lua数据类型转换成 json文本, json_decode 将 JSON格式的文本转换为LUA 的 TABLE.

```lua
	local lmu = require "lmu"
	
	local users={
		{['ID']=1,['username']='liigo.zhuang',['age']=26},
		{['ID']=2,['username']='bywayboy',['age']=26},
		{['ID']=3,['username']='sunwei',['age']=23},
	}
	
	-- lua table to json string.
	-- tow arguments, 
	--		arg 1 a lua variable.
	--		arg 2 boolean  
	---				true: encode utf8 to \uXXXX.
	---				false: unescape utf8 char.
	-- return 2 values.
	local succ, json_str = lmu.json_encode(users)
	print(succ,json_str)
	
	--- json string to lua table.
	local users = lmu.json_decode(json_str)
	for i,item in pairs(users) do
		print('ID='..item['ID'], 'username='..item['username'], 'age='..item['age'])
	end
	
	
```