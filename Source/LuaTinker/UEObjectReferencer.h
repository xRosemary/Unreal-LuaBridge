#ifndef FObjectReferencer_H
#define FObjectReferencer_H

#include "Containers/Map.h"
#include "UObject/GCObject.h"

/**
* 持有 UObject 的引用，避免其被GC
**/
class FObjectReferencer : public FGCObject
{
public:
    void AddObjectRef(UObject *Object)
    {
        ReferencedObjects.Add(Object, true);
    }

    void RemoveObjectRef(UObject *Object)
    {
        ReferencedObjects.Remove(Object);
    }

    void Cleanup()
    {
        return ReferencedObjects.Empty();
    }

    virtual void AddReferencedObjects(FReferenceCollector& Collector) override
    {
        Collector.AddReferencedObjects(ReferencedObjects);
    }

    virtual FString GetReferencerName() const override
    {
        return FString(TEXT("LuaObjectReferencer"));
    }

    static FObjectReferencer& Instance()
    {
        static FObjectReferencer Referencer;
        return Referencer;
    }

private:
    FObjectReferencer() {}

    TMap<UObject*, bool> ReferencedObjects;
};

#ifndef GObjectReferencer
#define GObjectReferencer FObjectReferencer::Instance()
#endif

#endif