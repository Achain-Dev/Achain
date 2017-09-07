#ifndef glua_structs_h
#define glua_structs_h

#include <string>
#include <sstream>

namespace glua {
	namespace decompile {

		/*
		** List is a doubly-linked list
		*/
		typedef struct ListItem_ ListItem;
		struct ListItem_ {
			ListItem* next;
			ListItem* prev;
			inline ListItem_()
			{
				next = nullptr;
				prev = nullptr;
			}
		};

		typedef struct List_ List;
		struct List_ {
			ListItem* head;
			ListItem* tail;
			int size;
			inline List_()
			{
				head = nullptr;
				tail = nullptr;
				size = 0;
			}
		};

		typedef void(*ListItemFn)(ListItem*, void*);
		typedef int(*ListItemCmpFn)(ListItem*, const void*);

		List* NewList();
		void InitList(List* list);
		void AddToList(List* list, ListItem* item);
		void AddToListHead(List* list, ListItem* item);
		ListItem* FirstItem(List* list);
		ListItem* LastItem(List* list);
		ListItem* PopFromList(List* list);
		void LoopList(List* list, ListItemFn fn, void* param);
		void ClearList(List* list, ListItemFn fn);
		ListItem* FindFromListHead(List* list, ListItemCmpFn cmp, const void* sample);
		ListItem* FindFromListTail(List* list, ListItemCmpFn cmp, const void* sample);
		ListItem* RemoveFromList(List* list, ListItem* item);
		int AddAllAfterListItem(List* list, ListItem* pos, ListItem* item);
		int AddAllBeforeListItem(List* list, ListItem* pos, ListItem* item);

		/*
		** IntSet is a Set that contains int
		*/
		typedef struct IntSet_ IntSet;
		struct IntSet_ {
			List list;
			int mayRepeat;
		};

		typedef struct IntSetItem_ IntSetItem;
		struct IntSetItem_ {
			ListItem super;
			int value;
		};

#define SET_SIZE(s) ((s)->list.size)
#define SET_IS_EMPTY(s) ((s)->list.size == 0)

		IntSet* NewIntSet(int mayRepeat);
		void InitIntSet(IntSet* set, int mayRepeat);
		void DeleteIntSet(IntSet* set);
		int AddToSet(IntSet* set, int a);
		int PeekSet(IntSet* set, int a);
		int PopSet(IntSet* set);
		int RemoveFromSet(IntSet* set, int a);

		/*
		** a Stack that use List
		*/
		typedef struct VarListItem_ VarListItem;
		struct VarListItem_ {
			ListItem super;
			std::string dest;
			std::string src;
			int reg;
			inline VarListItem_()
			{
				reg = 0;
			}
		};

		void AddToVarList(List* stack, std::string dest, std::string src, int reg);
		void ClearVarListItem(VarListItem* item, void* dummy);

	}
}

#endif // #ifndef glua_structs_h
