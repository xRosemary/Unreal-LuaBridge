local MyActor = UE.UClass()

function MyActor:TestFunc()
    UE.error("--------------")
    -- UE.dump()
    -- UE.error(self.__ClassDesc)
    -- UE.error(self.TestVar)
    -- UE.error(self.GetTestVar)
    -- UE.error(self.GetTestVar())
    -- UE.error(self.GetTestVar2())
    UE.error(self:TestFuncWithParam(123123, "abc", true))
    self:TestFuncWithParam2(123123, "abc", true)
    UE.dump()
    UE.error("--------------")
end

function MyActor:TestFuncWithParam2(Param1, Param2, Param3)
    UE.error(Param1)
    UE.error(Param2)
    UE.error(Param3)
    UE.error(self.TestVar)
    return 123
end

function MyActor:GetTestVar2()
    return 11
end

function MyActor:GetTestVar3()
    return 12
end

return MyActor