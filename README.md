LUA Modules
=========
����: ������ <http://blog.csdn.net/bywayboy>

C���� LUA ģ�������������:


*	һ������� LUAģ������.
*	һ��С�͵� JSON ����/������.

##ģ�������÷�ʾ��:

���´���չʾ��LUAģ�������ʹ�÷���.
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

###JSON����������÷�ʾ��:

```lua
	local lmu = require "lmu"
	
	local users={
		{['ID']=1,['username']='liigo.zhuang',['age']=26},
		{['ID']=2,['username']='bywayboy',['age']=26},
		{['ID']=3,['username']='sunwei',['age']=23},
	}
	
	-- lua table to json string.
	local succ, json_str = lmu.json_encode(users)
	print(succ,json_str)
	
	--- json string to lua table.
	local users = lmu.json_decode(json_str)
	for i,item in pairs(users) do
		print('ID='..item['ID'], 'username='..item['username'], 'age='..item['age'])
	end
	
	
```