local MyActor = UE.UClass()

function MyActor:TestFunc()
    UE.error("--------------")
    UE.dump()
    UE.error(self.__ClassDesc)
    UE.error(self.TestVar)
    UE.error(self.GetTestVar)
    UE.error(self.GetTestVar())
    UE.error(self.GetTestVar2())
    UE.dump()
    UE.error("--------------")
    return self.TestVar
end

function MyActor:GetTestVar()
    return 10
end

function MyActor:GetTestVar2()
    return 12
end

return MyActor