LUA Modules
=========
����: ������ <http://blog.csdn.net/bywayboy>

�����չ�����˼·��,һ�дӼ�, json Ĭ��֧��UTF8. ��û�б���ת��, ��� ��������������ַ�������LUAԴ���������UTF8 ����ſ�����������������������û�в��Թ�. C���� LUA ģ�������������:


*	һ������� LUAģ������.
*	һ��С�͵� JSON ����/������.

##����Ͱ�װ.

���ģ���ǻ���LUA 5.2 �����ġ���������Ҫ���� lua�Ŀ�. ������֮ǰ����Ҫ�Ȱ�װ cmake ���뷽��:

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


##ģ�������÷�ʾ��:

���´���չʾ��LUAģ�������ʹ�÷���.
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

##JSON����������÷�ʾ��:

JSON �����ṩ2������ json_encode �� lua��������ת���� json�ı�, json_decode �� JSON��ʽ���ı�ת��ΪLUA �� TABLE.

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