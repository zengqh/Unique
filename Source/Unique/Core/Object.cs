﻿using System;
using System.Collections.Generic;
using System.Text;

namespace Unique.Engine
{
    using System.Runtime.CompilerServices;
    using static Native;

    public class Object : RefCounted
    {
        public Object()
        {
        }

        public Object(IntPtr ptr) : base(ptr)
        {
        }

        public void SubscribeToEvent<T>(Action<T> action)
        {
            Object_SubscribeToGlobalEvent(native_, ref TypeOf<T>(), (receiver, eventType, eventData) =>
            {
                action(Utilities.As<T>(eventData));
            });
        }

        public void SubscribeToEvent<T>(Object sender, Action<T> action)
        {
            Object_SubscribeToEvent(native_, sender.native_, ref TypeOf<T>(), (r, et, ed) =>
            {
                action(Utilities.As<T>(ed));
            });
        }

        public void SubscribeToEvent<T>(ref StringID eventType, Action<T> action)
        {
            Object_SubscribeToGlobalEvent(native_, ref eventType, (r, et, ed) =>
            {
                action(Utilities.As<T>(ed));
            });
        }

        public void SubscribeToEvent<T>(Object sender, ref StringID eventType, Action<T> action)
        {
            Object_SubscribeToEvent(native_, sender.native_, ref eventType, (r, et, ed) =>
            {
                action(Utilities.As<T>(ed));
            });
        }
        
        public void SendEvent<T>(ref T eventData) where T : struct
        {
            Object_SendEvent(native_, ref TypeOf<T>(), Utilities.As(ref eventData));
        }
   
        public void SendEvent<T>(ref StringID eventType, ref T eventData) where T : struct
        {
            Object_SendEvent(native_, ref eventType, Utilities.As(ref eventData));
        }

        class TypeID<T>
        {
            public static StringID typeID = typeof(T).Name;
        }

        public static ref StringID TypeOf<T>()
        {
            return ref TypeID<T>.typeID;
        }

        public static Object CreateObject(StringID typeID, IntPtr nativePtr)
        {
            Type type = Type.GetType("Unique.Engine." + typeID);
            if(type == null)
            {
                return null;
            }
            Object obj = Activator.CreateInstance(type) as Object;
            if(!obj)
            {
                return null;
            }

            obj.Attach(nativePtr);
            return obj;
        }

        public static StringID GetNativeType(IntPtr nativePtr)
        {
            return new StringID(Object_GetType(nativePtr));
        }

        public static IntPtr CreateNative<T>()
        {
            return Object_Create(ref TypeOf<T>());
        }

        public static RefCounted PtrToObject(IntPtr nativePtr)
        {
            if(ptrToObject_.TryGetValue(nativePtr, out var weakRef))
            {
                if(weakRef.TryGetTarget(out var obj))
                    return obj;
            }

            StringID type = GetNativeType(nativePtr);

            return CreateObject(type, nativePtr);
        }

        public static T LookUpObject<T>(IntPtr nativePtr) where T : RefCounted, new()
        {
            if(ptrToObject_.TryGetValue(nativePtr, out var weakRef))
            {
                if(weakRef.TryGetTarget(out var obj))
                    return obj as T;
            }

            return null;
        }

        public static T PtrToObject<T>(IntPtr nativePtr) where T : RefCounted, new()
        {
            if(ptrToObject_.TryGetValue(nativePtr, out var weakRef))
            {
                if(weakRef.TryGetTarget(out var obj))
                    return obj as T;
            }

            StringID type = GetNativeType(nativePtr);
            T ret = new T();
            ret.Attach(nativePtr);
            return ret;
        }

    }

    public class Object<T> : Object
    {
        public Object() : base(CreateNative<T>())
        {
        }

        protected override void Destroy()
        {
            base.Destroy();

            if(native_ != IntPtr.Zero)
                Object_ReleaseRef(native_);
        }
    }

}