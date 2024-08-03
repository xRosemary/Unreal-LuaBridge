local MyActor = {}

function MyActor:TestFunc()
    UE.error("--------------")
    UE.dump()
    self.LuaVar = "TestLuaVar"

    local MyClass = UE.LoadClass("/Game/BP_TestOBJ.BP_TestOBJ_C")
    UE.error(MyClass)
    self.TestLuaObjVal = UE.NewObject(MyClass, self, "TestLuaObj")
    self.TestLuaObjVal:TestFunc()
    self.TestLuaObjVal = nil
    self.TestLuaObjVal2 = UE.NewObject(MyClass, self, "TestLuaObjWithRef")
    collectgarbage("collect")
    UE.error("--------------")
end

function MyActor:TestFuncWithParam2(Param1, Param2, Param3)
    if Param3 == false then
        self.TestLuaObjVal2:TestFunc()
        UE.error(Param3)
        return 654
    end

    self.TestVar = 987
    UE.error(self.TestVar)
    UE.error(self.LuaVar)
    UE.error(Param1)
    UE.error(Param2)
    UE.error(Param3)
    UE.error(self:GetTestVar2())
    UE.error(self:GetTestVar3())
    self:TestFuncWithParam2(Param1, Param2, false)
    UE.dump()
    return 123
end

function MyActor:GetTestVar2()
    return 11
end

function MyActor:GetTestVar3()
    return 12
end

return MyActor