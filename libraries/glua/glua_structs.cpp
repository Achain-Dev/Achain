#include <glua/glua_common.h>

#include <stdlib.h>
#include <assert.h>

#include <glua/lua.h>
#include <glua/glua_structs.h>
#include <glua/llimits.h>

namespace glua {
	namespace decompile {

		/*
		** List Functions
		*/

		List* NewList() {
			List* list = new List();
			return list;
		}

		void InitList(List* list) {
			list->head = nullptr;
			list->tail = nullptr;
			list->size = 0;
		}

		void AddToList(List* list, ListItem* item) {
			if (!list || !item) {
				return;
			}
			if (list->tail) {
				list->tail->next = item;
			}
			else {
				list->head = item;
			}
			item->prev = list->tail;
			item->next = NULL;
			list->tail = item;
			list->size++;
		}

		void AddToListHead(List* list, ListItem* item) {
			if (!list || !item) {
				return;
			}
			if (list->head) {
				list->head->prev = item;
			}
			else {
				list->tail = item;
			}
			item->next = list->head;
			item->prev = NULL;
			list->head = item;
			list->size++;
		}

		ListItem* FirstItem(List* list) {
			return list->head;
		}

		ListItem* LastItem(List* list) {
			return list->tail;
		}

		ListItem* PopFromList(List* list) {
			return RemoveFromList(list, list->tail);
		}

		void LoopList(List* list, ListItemFn fn, void* param) {
			ListItem* walk = list->head;
			while (walk) {
				ListItem* save = walk;
				walk = walk->next;
				fn(save, param);
			}
		}

		void ClearList(List* list, ListItemFn fn) {
			ListItem* walk = list->head;

			while (walk) {
				ListItem* save = walk;
				walk = walk->next;
				if (fn) {
					fn(save, NULL);
				}
				delete save;
			}

			list->head = NULL;
			list->tail = NULL;
			list->size = 0;
		}

		ListItem* FindFromListHead(List* list, ListItemCmpFn cmp, const void* sample) {
			ListItem* walk = list->head;
			while (walk) {
				if (cmp(walk, sample))
					return walk;
				walk = walk->next;
			}
			return NULL;
		}

		ListItem* FindFromListTail(List* list, ListItemCmpFn cmp, const void* sample) {
			ListItem* walk = list->tail;
			while (walk) {
				if (cmp(walk, sample))
					return walk;
				walk = walk->prev;
			}
			return NULL;
		}

		ListItem* RemoveFromList(List* list, ListItem* item) {
			if (!list || !item) {
				return NULL;
			}
			if (item->next) {
				item->next->prev = item->prev;
			}
			else if (list->tail == item) {
				list->tail = item->prev;
			}
			else {
				assert(0);
			}
			if (item->prev) {
				item->prev->next = item->next;
			}
			else if (list->head == item) {
				list->head = item->next;
			}
			else {
				assert(0);
			}
			list->size--;
			item->next = NULL;
			item->prev = NULL;
			return item;
		}

		int AddAllAfterListItem(List* list, ListItem* pos, ListItem* item) {
			ListItem* last = NULL;
			int count = 0;
			if (!list || !item) {
				return count;
			}
			last = item;
			count = 1;
			while (last->next) {
				count++;
				last = last->next;
			}
			item->prev = pos;
			if (pos == NULL) {
				last->next = list->head;
				list->head = item;
			}
			else {
				last->next = pos->next;
				pos->next = item;
			}
			if (last->next == NULL) {
				list->tail = last;
			}
			else {
				last->next->prev = last;
			}
			list->size += count;
			return count;
		}

		int AddAllBeforeListItem(List* list, ListItem* pos, ListItem* item) {
			ListItem* last = NULL;
			int count = 0;
			if (!list || !item) {
				return count;
			}
			last = item;
			count = 1;
			while (last->next) {
				count++;
				last = last->next;
			}
			last->next = pos;
			if (pos == nullptr) {
				item->prev = list->tail;
				list->tail = last;
			}
			else {
				item->prev = pos->prev;
				pos->prev = last;
			}
			if (item->prev == nullptr) {
				list->head = item;
			}
			else {
				item->prev->next = item;
			}
			list->size += count;
			return count;
		}

		/*
		** IntSet Functions
		*/

		IntSet* NewIntSet(int mayRepeat) {
			auto set = new IntSet();
			InitIntSet(set, mayRepeat);
			return set;
		}

		void InitIntSet(IntSet* set, int mayRepeat) {
			set->mayRepeat = mayRepeat;
			InitList(&(set->list));
		}

		void DeleteIntSet(IntSet* set) {
			ClearList(&(set->list), nullptr);
			delete set;
		}

		int AddToSet(IntSet* set, int a) {
			IntSetItem* item;
			if (!set->mayRepeat) {
				ListItem* ptr = set->list.head;
				while (ptr) {
					if (lua_cast(IntSetItem*, ptr)->value == a) {
						return 0;
					}
					ptr = ptr->next;
				}
			}
			item = new IntSetItem();
			item->value = a;
			AddToList(&(set->list), (ListItem*)item);
			return 1;
		}

		int PeekSet(IntSet* set, int a) {
			ListItem* ptr = set->list.head;
			while (ptr) {
				if (lua_cast(IntSetItem*, ptr)->value == a) {
					return 1;
					break;
				}
				ptr = ptr->next;
			}
			return 0;
		}

		int PopSet(IntSet* set) {
			int val;
			ListItem* item = PopFromList(&(set->list));
			if (item == NULL) {
				return 0;
			}
			val = lua_cast(IntSetItem*, item)->value;
			delete item;
			return val;
		}

		int RemoveFromSet(IntSet* set, int a) {
			ListItem* ptr = set->list.head;
			while (ptr) {
				if (lua_cast(IntSetItem*, ptr)->value == a) {
					break;
				}
				ptr = ptr->next;
			}
			if (ptr == NULL) {
				return 0;
			}
			delete RemoveFromList(&(set->list), ptr);
			return 1;
		}

		/*
		** VarList Functions
		*/

		void AddToVarList(List* stack, std::string dest, std::string src, int reg) {
			VarListItem* var = new VarListItem();
			var->dest = dest;
			var->src = src;
			var->reg = reg;
			AddToList(stack, (ListItem*)var);
		}

		void ClearVarListItem(VarListItem* item, void* dummy) {
			if (item) {
				item->src = "";
				item->dest = "";
			}
		}

	}
}