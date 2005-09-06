//
// JSUtils.cpp
//

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"
#include "JSRun.h"
#include "UserObjectImp.h"
#include "JSValueWrapper.h"
#include "JSObject.h"

struct ObjectImpList {
	ObjectImp* imp;
	ObjectImpList* next;
	CFTypeRef data;
};

static CFTypeRef KJSValueToCFTypeInternal(const Value& inValue, ExecState *exec, ObjectImpList* inImps);


//--------------------------------------------------------------------------
//	CFStringToUString
//--------------------------------------------------------------------------

UString CFStringToUString(CFStringRef inCFString)
{
	UString result;
    if (inCFString)
    {
        CFIndex len = CFStringGetLength(inCFString);
        UniChar* buffer = (UniChar*)malloc(sizeof(UniChar) * len);
        if (buffer)
        {
            CFStringGetCharacters(inCFString, CFRangeMake(0, len), buffer);
            result = UString((const UChar *)buffer, len);
            free(buffer);
        }
    }
	return result;
}


//--------------------------------------------------------------------------
//	UStringToCFString
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFStringRef
CFStringRef UStringToCFString(const UString& inUString)
{
	return CFStringCreateWithCharacters(NULL, (const UniChar*)inUString.data(), inUString.size());
}


#if JAG_PINK_OR_LATER
//--------------------------------------------------------------------------
//	CFStringToIdentifier
//--------------------------------------------------------------------------

Identifier CFStringToIdentifier(CFStringRef inCFString)
{
	return Identifier(CFStringToUString(inCFString));
}


//--------------------------------------------------------------------------
//	IdentifierToCFString
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFStringRef
CFStringRef IdentifierToCFString(const Identifier& inIdentifier)
{
	return UStringToCFString(inIdentifier.ustring());
}
#endif


//--------------------------------------------------------------------------
//	KJSValueToJSObject
//--------------------------------------------------------------------------
JSUserObject*		KJSValueToJSObject(const Value& inValue, ExecState *exec)
{
	JSUserObject* result = NULL;
#if JAG_PINK_OR_LATER
	UserObjectImp* userObjectImp;
	if (inValue.type() == ObjectType && (userObjectImp = dynamic_cast<UserObjectImp*>(inValue.imp())))
#else
	if (UserObjectImp* userObjectImp = dynamic_cast<UserObjectImp*>(inValue.imp()))
#endif
	{
		result =  userObjectImp->GetJSUserObject();
		if (result) result->Retain();
	}
	else
	{
		JSValueWrapper* wrapperValue = new JSValueWrapper(inValue, exec);
		if (wrapperValue)
		{
			JSObjectCallBacks callBacks;
			JSValueWrapper::GetJSObectCallBacks(callBacks);
			result = (JSUserObject*)JSObjectCreate(wrapperValue, &callBacks);
			if (!result)
			{
				delete wrapperValue;
			}
		}
	}
	return result;
}

//--------------------------------------------------------------------------
//	JSObjectKJSValue
//--------------------------------------------------------------------------
Value JSObjectKJSValue(JSUserObject* ptr)
{
    Value result = Undefined();
    if (ptr)
    {
        bool handled = false;
        
        switch (ptr->DataType())
        {
            case kJSUserObjectDataTypeJSValueWrapper:
            {
                JSValueWrapper* wrapper = (JSValueWrapper*)ptr->GetData();
                if (wrapper)
                {
                    result = wrapper->GetValue();
                    handled = true;
                }
                break;
            }
                
            case kJSUserObjectDataTypeCFType:
            {
                CFTypeRef cfType = (CFTypeRef*)ptr->GetData();
                if (cfType)
                {
                    CFTypeID typeID = CFGetTypeID(cfType);
                    if (typeID == CFStringGetTypeID())
                    {
                        result = String(CFStringToUString((CFStringRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFNumberGetTypeID())
                    {
                        if (CFNumberIsFloatType((CFNumberRef)cfType))
                        {
                            double num;
                            if (CFNumberGetValue((CFNumberRef)cfType, kCFNumberDoubleType, &num))
                            {
                                result = Number(num);
                                handled = true;
                            }
                        }
                        else
                        {
                            long num;
                            if (CFNumberGetValue((CFNumberRef)cfType, kCFNumberLongType, &num))
                            {
                                result = Number(num);
                                handled = true;
                            }
                        }
                    }
                    else if (typeID == CFBooleanGetTypeID())
                    {
                        result = KJS::Boolean(CFBooleanGetValue((CFBooleanRef)cfType));
                        handled = true;
                    }
                    else if (typeID == CFDateGetTypeID())
                    {
                    }
                    else if (typeID == CFNullGetTypeID())
                    {
                        result = Null();
                        handled = true;
                    }
                }
                break;
            }
        }
        if (!handled)
        {
            result = Object(new UserObjectImp(ptr));
        }
    }
    return result;
}




//--------------------------------------------------------------------------
//	KJSValueToCFTypeInternal
//--------------------------------------------------------------------------
// Caller is responsible for releasing the returned CFTypeRef
CFTypeRef KJSValueToCFTypeInternal(const Value& inValue, ExecState *exec, ObjectImpList* inImps)
{
#if JAG_PINK_OR_LATER
	if (inValue.isNull())
		return NULL;
#endif
		
	CFTypeRef result = NULL;
	
	switch (inValue.type())
	{
		case BooleanType:
			{
				result = inValue.toBoolean(exec) ? kCFBooleanTrue : kCFBooleanFalse;
				RetainCFType(result);
			}
			break;
			
		case StringType:
			{
				UString uString = inValue.toString(exec);
				result = UStringToCFString(uString);
			}
			break;
			
		case NumberType:
			{
				double number1 = inValue.toNumber(exec);
				double number2 = (double)inValue.toInteger(exec);
				if (number1 ==  number2)
				{
					int intValue = (int)number2;
					result = CFNumberCreate(NULL, kCFNumberIntType, &intValue);
				}
				else
				{
					result = CFNumberCreate(NULL, kCFNumberDoubleType, &number1);
				}
			}
			break;
			
		case ObjectType:
			{
				if (UserObjectImp* userObjectImp = dynamic_cast<UserObjectImp*>(inValue.imp()))
				{
					JSUserObject* ptr = userObjectImp->GetJSUserObject();
					if (ptr)
					{
						result = ptr->CopyCFValue();
					}
				}
				else
				{
					Object object = inValue.toObject(exec);
					UInt8 isArray = false;

					// if two objects reference each
					ObjectImp* imp = object.imp();
					ObjectImpList* temp = inImps;
					while (temp) {
						if (imp == temp->imp) {
							return CFRetain(GetCFNull());
						}
						temp = temp->next;
					}

					ObjectImpList imps;
					imps.next = inImps;
					imps.imp = imp;

					
//[...] HACK since we do not have access to the class info we use class name instead
#if 0
					if (object.inherits(&ArrayInstanceImp::info))
#else
					if (object.className() == "Array")
#endif
					{
						isArray = true;					
#if JAG_PINK_OR_LATER
						JSInterpreter* intrepreter = (JSInterpreter*)exec->dynamicInterpreter();
						if (intrepreter && (intrepreter->Flags() & kJSFlagConvertAssociativeArray)) {
							ReferenceList propList = object.propList(exec, false);
							ReferenceListIterator iter = propList.begin();
							ReferenceListIterator end = propList.end();
							while(iter != end && isArray)
							{
								Identifier propName = iter->getPropertyName(exec);
								UString ustr = propName.ustring();
								const UniChar* uniChars = (const UniChar*)ustr.data();
								int size = ustr.size();
								while (size--) {
									if (uniChars[size] < '0' || uniChars[size] > '9') {
										isArray = false;
										break;
									}
								}
								iter++;
							}
						}
#endif
					}
					
					if (isArray)
					{				
						// This is an KJS array
						unsigned int length = object.get(exec, "length").toUInt32(exec);
						result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
						if (result)
						{
#if JAG_PINK_OR_LATER
							for (unsigned i = 0; i < length; i++)
							{
								CFTypeRef cfValue = KJSValueToCFTypeInternal(object.get(exec, i), exec, &imps);
								CFArrayAppendValue((CFMutableArrayRef)result, cfValue);
								ReleaseCFType(cfValue);
							}
#else
							for (unsigned int i = 0; i < length; i++)
							{
								UString propertyName = UString::from(i);
								CFTypeRef cfValue = KJSValueToCFTypeInternal(object.get(exec, propertyName), exec, &imps);
								CFArrayAppendValue((CFMutableArrayRef)result, cfValue);
								ReleaseCFType(cfValue);
							}
#endif
						}
					}
					else
					{
#if JAG_PINK_OR_LATER
						// Not an arry, just treat it like a dictionary which contains (property name, property value) paiars
						ReferenceList propList = object.propList(exec, false);
						{
							result = CFDictionaryCreateMutable(NULL, 
															   0, 
															   &kCFTypeDictionaryKeyCallBacks, 
															   &kCFTypeDictionaryValueCallBacks);
							if (result)
							{
								ReferenceListIterator iter = propList.begin();
								ReferenceListIterator end = propList.end();
								while(iter != end)
								{
									Identifier propName = iter->getPropertyName(exec);
									if (object.hasProperty(exec, propName))
									{
										CFStringRef cfKey = IdentifierToCFString(propName);
										CFTypeRef cfValue = KJSValueToCFTypeInternal(object.get(exec, propName), exec, &imps);
										if (cfKey && cfValue)
										{
											CFDictionaryAddValue((CFMutableDictionaryRef)result, cfKey, cfValue);
										}
										ReleaseCFType(cfKey);
										ReleaseCFType(cfValue);
									}
									iter++;
								}
							}
						}
#else
						List propList = object.propList(exec);
						if (propList.size() > 0)
						{
							result = CFDictionaryCreateMutable(NULL, 
															   0, 
															   &kCFTypeDictionaryKeyCallBacks, 
															   &kCFTypeDictionaryValueCallBacks);
							if (result)
							{
								ListIterator iter = propList.begin();
								ListIterator end = propList.end();
								while(iter != end)
								{
									UString propName = iter->getPropertyName(exec);
									if (object.hasProperty(exec, propName))
									{
										CFStringRef cfKey = UStringToCFString(propName);
										CFTypeRef cfValue = KJSValueToCFTypeInternal(iter->getValue(exec), exec, &imps);
										if (cfKey && cfValue)
										{
											CFDictionaryAddValue((CFMutableDictionaryRef)result, cfKey, cfValue);
										}
										ReleaseCFType(cfKey);
										ReleaseCFType(cfValue);
									}
									++iter;
								}
							}
						}
#endif
					}
				}
			}
			break;

#if !JAG_PINK_OR_LATER
		case ReferenceType:
			{
				Value value = inValue.getValue(exec);
				result = KJSValueToCFTypeInternal(value, exec, NULL);
			}
			break;

		case ListType:
			{
				List list = List::dynamicCast(inValue);
				if (!list.isNull())
				{
					result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
					if (result)
					{
						ListIterator iter = list.begin();
						ListIterator end = list.end();
						while (iter != end)
						{
							CFTypeRef cfTypeRef = KJSValueToCFTypeInternal(*iter, exec, NULL);
							if (cfTypeRef)
								CFArrayAppendValue((CFMutableArrayRef)result, cfTypeRef);
							++iter;
						}
					}
				}
			}
			break;
#endif
		
		case NullType:
		case UndefinedType:
		case UnspecifiedType:
			result = RetainCFType(GetCFNull());
			break;
			
#if !JAG_PINK_OR_LATER
		case CompletionType:
			{
				Completion completion = Completion::dynamicCast(inValue);
				if (completion.isValueCompletion())
				{
					result = KJSValueToCFTypeInternal(completion.value(), exec, NULL);
				}
			}
			break;
#endif

#if JAG_PINK_OR_LATER
		default:
			fprintf(stderr, "KJSValueToCFType: wrong value type %d\n", inValue.type());
			break;
#endif
	}
	
	return result;
}

CFTypeRef KJSValueToCFType(const Value& inValue, ExecState *exec)
{
	return KJSValueToCFTypeInternal(inValue, exec, NULL);
}

CFTypeRef GetCFNull(void)
{
	static CFArrayRef sCFNull = CFArrayCreate(NULL, NULL, 0, NULL);
	CFTypeRef result = JSGetCFNull();
	if (!result)
	{
		result = sCFNull;
	}
	return result;
}

