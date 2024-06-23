
#define UNION_MEMBER(type, name) type name[VALUE_SIZE/sizeof(type)]

#define CAT_TOKEN0(x,y) x##y
#define CAT_TOKEN2(x,y) CAT_TOKEN0(x,y)
#define CAT_TOKEN3(a,b,c) CAT_TOKEN2(CAT_TOKEN2(a,b),c)
#define CAT_TOKEN4(a,b,c,d) CAT_TOKEN2(CAT_TOKEN3(a,b,c),d)
#define CAT_TOKEN5(a,b,c,d,e) CAT_TOKEN2(CAT_TOKEN4(a,b,c,d),e)

#define VAL_LESS_THAN(x,y) (GET_KEY(x) < GET_KEY(y))
#define VAL_EQUAL(x,y) (GET_KEY(x) == GET_KEY(y))

#define ASSIGN_FROM(px, py) *(TempBuf*)(px) = *(TempBuf*)(py)
#define DEFINE_FROM(x, py) TempBuf x = *(TempBuf*)(py)


