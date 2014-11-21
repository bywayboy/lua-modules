local util = {}
function util.tpl(writer)
	local t =  require "cfmt.tpl"
	local tpl = {}
	local VARS = {} -- save the assigned variables.
	local _prefix
	local _suffix
	-- 创建的时候指定 writer
	if nil ~= writer then VARS['_']=writer end
	
	function tpl:assign(name,value)
		VARS[name]=value
	end
	
	function tpl:boundary(prefix, suffix)
		if nil ~= prefix then _prefix=prefix end
		if nil ~= suffix then _suffix=suffix end
	end
	
	function tpl:display(tpl,cache,writer)

		if nil~=writer then VARS['_']=writer end
		local w = VARS['_']
		VARS['usetime']=0
		exectime = os.clock()
		local code =t.compile(tpl,cache, VARS, "_", _prefix, _suffix)
		VARS['usetime']=string.format("%2f",(os.clock()-exectime) * 1000)
		load(code)(VARS) -- run compiled tpl code.
	end
	return tpl;
end


return util