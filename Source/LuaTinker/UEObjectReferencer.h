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
    void AddObjectRef(UObject *Object, int RefKey)
    {
        if(Object == NULL)
        {
            return;
        }

        ReferencedObjects.Add(Object, RefKey);
    }

    int* FindObjectRef(UObject *Object)
    {
        return ReferencedObjects.Find(Object);
    }

    void RemoveObjectRef(UObject *Object, int* RefKey)
    {
        ReferencedObjects.RemoveAndCopyValue(Object, *RefKey);
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

    TMap<UObject*, int> ReferencedObjects;
};

#ifndef GObjectReferencer
#define GObjectReferencer FObjectReferencer::Instance()
#endif

#endif