local MyActor = {}

function MyActor:TestFunc()
    UE.error("--------------")
    UE.dump()
    -- UE.error(self.__ClassDesc)
    -- UE.error(self.TestVar)
    -- UE.error(self.GetTestVar)
    -- UE.error(self.GetTestVar())
    -- UE.error(self.GetTestVar2())
    -- UE.error(self:TestFuncWithParam(123123, "abc", true))
    -- UE.error(self.TestFuncWithParam)
    self.LuaVar = "TestLuaVar"
    -- self.TestFuncWithParam2(123123, "abc", true)
    -- UE.error(self:TestFuncWithParam2(123123, "abc", true))
    -- UE.error(MyActor.TestFuncWithParam2)
    UE.error("--------------")
end

function MyActor:TestFuncWithParam2(Param1, Param2, Param3)
    if Param3 == false then
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