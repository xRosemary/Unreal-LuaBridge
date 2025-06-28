#ifndef UObjectReferencer_H
#define UObjectReferencer_H

#include "../Core.h"
#include "../UnClass.h"

/**
 * 持有 UObject 的引用，避免其被GC
 **/
class CORE_API UObjectReferencer : public UObject
{
public:
    DECLARE_CLASS(UObjectReferencer, UObject, CLASS_Transient, Core)

    void AddObjectRef(UObject *Object, int RefKey)
    {
        if (Object == NULL)
        {
            return;
        }

        ReferencedObjects.Set(Object, RefKey);
    }

    int *FindObjectRef(UObject *Object)
    {
        return ReferencedObjects.Find(Object);
    }

    bool RemoveObjectRef(UObject *Object, int *RefKey)
    {
        guard(UObjectReferencer::RemoveObjectRef, 891HU0wTDoKqlhHUoU1OSuyt6hrrkPmi);
        if (Object == NULL)
        {
            return false;
        }
        
        int *TempRefKey = ReferencedObjects.Find(Object);
        if (TempRefKey != NULL)
        {
            *RefKey = *TempRefKey;
            ReferencedObjects.Remove(Object);
            return true;
        }

        return false;
        unguard;
    }

    void Cleanup()
    {
        ReferencedObjects.Empty();
    }

    virtual void Serialize(FArchive &Ar)
    {
        guard(UObjectReferencer::Serialize, CPpKnh0T1fo6vjH4GKIjyzTwcyOAu90X);
        Super::Serialize(Ar);
        Ar << ReferencedObjects;
        unguard;
    }

    static UObjectReferencer *Instance()
    {
        guard(UObjectReferencer::Instance, J4Fp3H746SEHmaZsrUQb3HDAQl2UFf5S);
        static UObjectReferencer *Referencer = NULL;
        if (Referencer == NULL)
        {
            Referencer = ConstructObject<UObjectReferencer>(UObjectReferencer::StaticClass());
            Referencer->AddToRoot();
        }
        return Referencer;
        unguard;
    }

    static void ReleaseEnv()
    {
        Instance()->RemoveFromRoot();
    }

private:
    TMap<UObject *, int> ReferencedObjects;
};

#ifndef GObjectReferencer
#define GObjectReferencer UObjectReferencer::Instance()
#endif

#endif