#include <stdlib.h>

#include "ADTMap.h"

// Κάθε θέση i θεωρείται γεινοτική με όλες τις θέσεις μέχρι και την i + NEIGHBOURS
#define NEIGHBOURS 3

// Οι κόμβοι του map στην υλοποίηση με hash table, μπορούν να είναι σε 3 διαφορετικές καταστάσεις,
// ώστε αν διαγράψουμε κάποιον κόμβο, αυτός να μην είναι empty, ώστε να μην επηρεάζεται η αναζήτηση
// αλλά ούτε occupied, ώστε η εισαγωγή να μπορεί να το κάνει overwrite.
typedef enum {
	EMPTY, OCCUPIED
} State;

// Το μέγεθος του Hash Table ιδανικά θέλουμε να είναι πρώτος αριθμός σύμφωνα με την θεωρία.
// Η παρακάτω λίστα περιέχει πρώτους οι οποίοι έχουν αποδεδιγμένα καλή συμπεριφορά ως μεγέθη.
// Κάθε re-hash θα γίνεται βάσει αυτής της λίστας. Αν χρειάζονται παραπάνω απο 1610612741 στοχεία, τότε σε καθε rehash διπλασιάζουμε το μέγεθος.
int prime_sizes[] = {53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241,
	786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

// Χρησιμοποιούμε open addressing, οπότε σύμφωνα με την θεωρία, πρέπει πάντα να διατηρούμε
// τον load factor του  hash table μικρότερο ή ίσο του 0.5, για να έχουμε αποδoτικές πράξεις
#define MAX_LOAD_FACTOR 0.5

// Δομή του κάθε κόμβου που έχει το hash table (με το οποίο υλοιποιούμε το map)
struct map_node{
	Pointer key;		// Το κλειδί που χρησιμοποιείται για να hash-αρουμε
	Pointer value;  	// Η τιμή που αντισοιχίζεται στο παραπάνω κλειδί
	State state;		// Μεταβλητή για να μαρκάρουμε την κατάσταση των κόμβων (βλέπε διαγραφή)
};

// Δομή του Map (περιέχει όλες τις πληροφορίες που χρεαζόμαστε για το HashTable)
struct map {
	MapNode array;				// Ο πίνακας που θα χρησιμοποιήσουμε για το map (remember, φτιάχνουμε ένα hash table)
	int capacity;				// Πόσο χώρο έχουμε δεσμεύσει.
	int size;					// Πόσα στοιχεία έχουμε προσθέσει
	CompareFunc compare;		// Συνάρτηση για σύγκριση δεικτών, που πρέπει να δίνεται απο τον χρήστη
	HashFunc hash_function;		// Συνάρτηση για να παίρνουμε το hash code του κάθε αντικειμένου.
	DestroyFunc destroy_key;	// Συναρτήσεις που καλούνται όταν διαγράφουμε έναν κόμβο απο το map.
	DestroyFunc destroy_value;
};


Map map_create(CompareFunc compare, DestroyFunc destroy_key, DestroyFunc destroy_value) {
	// Δεσμεύουμε κατάλληλα τον χώρο που χρειαζόμαστε για το hash table
	Map map = malloc(sizeof(*map));
	map->capacity = prime_sizes[0];
	map->array = malloc(map->capacity * sizeof(struct map_node));

	// Αρχικοποιούμε τους κόμβους που έχουμε σαν διαθέσιμους.
	for (int i = 0; i < map->capacity; i++)
		map->array[i].state = EMPTY;

	map->size = 0;
	map->compare = compare;
	map->destroy_key = destroy_key;
	map->destroy_value = destroy_value;

	return map;
}

// Επιστρέφει τον αριθμό των entries του map σε μία χρονική στιγμή.
int map_size(Map map) {
	return map->size;
}

// Συνάρτηση για την επέκταση του Hash Table σε περίπτωση που ο load factor μεγαλώσει πολύ.
static void rehash(Map map) {
	// Αποθήκευση των παλιών δεδομένων
	int old_capacity = map->capacity;
	MapNode old_array = map->array;

	// Βρίσκουμε τη νέα χωρητικότητα, διασχίζοντας τη λίστα των πρώτων ώστε να βρούμε τον επόμενο. 
	int prime_no = sizeof(prime_sizes) / sizeof(int);	// το μέγεθος του πίνακα
	for (int i = 0; i < prime_no; i++) {					// LCOV_EXCL_LINE
		if (prime_sizes[i] > old_capacity) {
			map->capacity = prime_sizes[i]; 
			break;
		}
	}
	// Αν έχουμε εξαντλήσει όλους τους πρώτους, διπλασιάζουμε
	if (map->capacity == old_capacity)					// LCOV_EXCL_LINE
		map->capacity *= 2;								// LCOV_EXCL_LINE

	// Δημιουργούμε ένα μεγαλύτερο hash table
	map->array = malloc(map->capacity * sizeof(struct map_node));
	for (int i = 0; i < map->capacity; i++)
		map->array[i].state = EMPTY;

	// Τοποθετούμε ΜΟΝΟ τα entries που όντως περιέχουν ένα στοιχείο (το rehash είναι και μία ευκαιρία να ξεφορτωθούμε τα deleted nodes)
	map->size = 0;
	for (int i = 0; i < old_capacity; i++)
		if (old_array[i].state == OCCUPIED)
			map_insert(map, old_array[i].key, old_array[i].value);

	//Αποδεσμεύουμε τον παλιό πίνακα ώστε να μήν έχουμε leaks
	free(old_array);
}

// Εισαγωγή στο hash table του ζευγαριού (key, item). Αν το key υπάρχει,
// ανανέωση του με ένα νέο value, και η συνάρτηση επιστρέφει true.
void map_insert(Map map, Pointer key, Pointer value) {
    // Σκανάρουμε το Hash Table μέχρι να βρούμε διαθέσιμη θέση για να τοποθετήσουμε το ζευγάρι,
    // ή μέχρι να βρούμε το κλειδί ώστε να το αντικαταστήσουμε.
    MapNode node = map_find_node(map, key);
    if(node != MAP_EOF){
        if (map->destroy_key != NULL)
            map->destroy_key(node->key);
        if (map->destroy_value != NULL)
            map->destroy_value(node->value);
        node->key = key;
        node->value = value;
        return;
    }

    uint pos = map->hash_function(key) % map->capacity;		// Βρίσκουμε τη θέση που χασάρει το key
    if(map->array[pos].state == EMPTY){						// Αν στη θέση αυτη ο κόμβοσ είναι κενός τοποθετούμε το στοιχείο
        map->array[pos].key = key;
        map->array[pos].value = value;
        map->array[pos].state = OCCUPIED;
        map->size++;										// Αυξάνουμε το μέγεθος του map
		float load_factor = (float)(map->size ) / map->capacity;// Πριν επιστρέψουμε ελέγχουμε αν χρειάζεται rehash
   			if (load_factor > MAX_LOAD_FACTOR)
        		rehash(map);
        return;
    }
    // Διαφορετικά ψάχνουμε την επόμενη κενή θέση , η οποία βρίσκεται στη μεταβλητή e
    uint e = pos +1;
    while(e!=pos){
        if(map->array[e].state == EMPTY){
            break;
        }
        e = (e + 1) % map->capacity;
    }
    
    while(1){   					// Η επανάληψη συνεχίζεται μέχρι να γίνει κάποιο return μέσα στο loop
        if(e<pos) e = e + map->capacity;	// Για να γίνει σωστά η παρακάτω πράξη με το απόλυτο
       // Αν η διαφόρα της κενής θέσης και της θέσης που χασάρει το key είναι μικρότερη ή ίση απο NEIGHBOURS 
	   // τότε η θέση e είναι γειτονική και το στοιχείο θα εισαχθεί εκεί
	    if(abs(e - pos) <= NEIGHBOURS){	
            e = e % map->capacity;	// Επαναφέρουμε το e στη πραγματική θέση που αντιστοιχεί στο map
            map->array[e].key = key;
            map->array[e].value = value;
            map->array[e].state = OCCUPIED;
            map->size++;
			float load_factor = (float)(map->size ) / map->capacity;
   				if (load_factor > MAX_LOAD_FACTOR)
        			rehash(map);
            return;
        }
		// Ψάχνουμε να βρούμε αν μπορεί να γίνει swap με κάποιο απο τα NEIGHBOURS γειτονικά στοιχεία
        uint count = 0;				
        while(count<NEIGHBOURS){					// To loop θα γίνει όσες φορές και τα γειτονικά στοιχεία
            uint i = (e - NEIGHBOURS + count) % map->capacity; // i η θέση του εκάστοτε γειτονικού στοιχείου
            uint hashi = map->hash_function(map->array[i].key) % map->capacity ; // hashi η θέση που χασάρει το στοιχείο που βρίσκεται στη θέση i
			if(e<pos) e = e + map->capacity;				// Για να γίνει σωστά η παρακάτω πράξη με το απόλυτο
			if(hashi<pos) hashi = hashi + map->capacity;	// Για να γίνει σωστά η παρακάτω πράξη με το απόλυτο
             
			 // Αν η διαφόρα της κενής θέσης(e) και της θέσης που χασάρει το στοιχείο που βρίσκεται στη θέση i(hashi), 
			 // είναι μικρότερη ή ίση απο NEIGHBOURS , τότε μπορούμε να κάνουμε swap τα δύο στοιχεία,
			 // αφού το στοιχείο στη θέση i θα εξακολουθεί να είναι γειτονικό στοιχείο με τη θέση hashi
			if(abs(e - hashi) <= NEIGHBOURS){
                e = e % map->capacity;
                map->array[e].key = map->array[i].key;		 // Κάνουμε το swap
                map->array[e].value = map->array[i].value;
                map->array[e].state = OCCUPIED;
                map->array[i].state = EMPTY;				// Δημιουργούμε τη νέα ελεύθερη θέση
				map->array[i].key = NULL;
				map->array[i].value = NULL;
                e = i;
                break;		// break και συνεχίζουμε να φτιάχνουμε κενές θέσεις κοντά στο pos
            }
            count++;		// Διαφορετικά εξετάζουμε το επόμενο γειτονικό στοιχείο
        }
		// Αν δεν μπορεί να γίνει swap με κάποιο απο τα NEIGHBOURS γειτονικά στοιχεία κάνουμε rehash
        if(count>=NEIGHBOURS){
            rehash(map);
			pos = map->hash_function(key) % map->capacity;	// Βρίσκουμε τη θέση που χασάρει το key στο νέο map
            if(map->array[pos].state == EMPTY){				// Αν στη θέση αυτη ο κόμβοσ είναι κενός τοποθετούμε το στοιχείο
                map->array[pos].key = key;
                map->array[pos].value = value;
                map->array[pos].state = OCCUPIED;
                map->size++;			// Αυξάνουμε το μέγεθος του map αλλά δεν ελέγχουμε για rehash γιατι μόλις έγινε
                return;										
            }
            // Διαφορετικά ψάχνουμε την επόμενη κενή θέση
            e = pos +1;
            while(e!=pos){
                if(map->array[e].state == EMPTY){
                    break;
                }
                e = (e + 1) % map->capacity;
            }
        }
    }
}


// Διαγραφή απο το Hash Table του κλειδιού με τιμή key
bool map_remove(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node == MAP_EOF)
		return false;

	// destroy
	if (map->destroy_key != NULL)
		map->destroy_key(node->key);
	if (map->destroy_value != NULL)
		map->destroy_value(node->value);

	
	node->state = EMPTY;		// Όταν γίνει η αφαίρεση ο κόμβος θα έιναι empty
	map->size--;

	return true;
}

// Αναζήτηση στο map, με σκοπό να επιστραφεί το value του κλειδιού που περνάμε σαν όρισμα.
Pointer map_find(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node != MAP_EOF)
		return node->value;
	else
		return NULL;
}


DestroyFunc map_set_destroy_key(Map map, DestroyFunc destroy_key) {
	DestroyFunc old = map->destroy_key;
	map->destroy_key = destroy_key;
	return old;
}

DestroyFunc map_set_destroy_value(Map map, DestroyFunc destroy_value) {
	DestroyFunc old = map->destroy_value;
	map->destroy_value = destroy_value;
	return old;
}

// Απελευθέρωση μνήμης που δεσμεύει το map
void map_destroy(Map map) {
	for (int i = 0; i < map->capacity; i++) {
		if (map->array[i].state == OCCUPIED) {
			if (map->destroy_key != NULL)
				map->destroy_key(map->array[i].key);
			if (map->destroy_value != NULL)
				map->destroy_value(map->array[i].value);
		}
	}

	free(map->array);
	free(map);
}

/////////////////////// Διάσχιση του map μέσω κόμβων ///////////////////////////

MapNode map_first(Map map) {
	//Ξεκινάμε την επανάληψή μας απο το 1ο στοιχείο, μέχρι να βρούμε κάτι όντως τοποθετημένο
	for (int i = 0; i < map->capacity; i++)
		if (map->array[i].state == OCCUPIED)
			return &map->array[i];

	return MAP_EOF;
}

MapNode map_next(Map map, MapNode node) {
	// Το node είναι pointer στο i-οστό στοιχείο του array, οπότε node - array == i  (pointer arithmetic!)
	for (int i = node - map->array + 1; i < map->capacity; i++)
		if (map->array[i].state == OCCUPIED)
			return &map->array[i];

	return MAP_EOF;
}

Pointer map_node_key(Map map, MapNode node) {
	return node->key;
}

Pointer map_node_value(Map map, MapNode node) {
	return node->value;
}

MapNode map_find_node(Map map, Pointer key) {
	uint pos =  map->hash_function(key) % map->capacity;	// Βρίσκουμε τη θέση που χασάρει το key
	for (int i = 0;	i<= NEIGHBOURS;	i++){					// Ψάχνουμε αν το κλειδί key βρίσκεται σε γειτονικό κόμβο η στη θέση pos
		if (map->array[pos].state == OCCUPIED && map->compare(map->array[pos].key, key) == 0){
			return &map->array[pos];						// Bρέθηκε και το επιστέφουμε
		}
		pos = (pos + 1) % map->capacity;        
	}
	// Διαφορετικά δεν υπαρχει καθόλου στο map
	return MAP_EOF;
}

// Αρχικοποίηση της συνάρτησης κατακερματισμού του συγκεκριμένου map.
void map_set_hash_function(Map map, HashFunc func) {
	map->hash_function = func;
}

uint hash_string(Pointer value) {
	// djb2 hash function, απλή, γρήγορη, και σε γενικές γραμμές αποδοτική
    uint hash = 5381;
    for (char* s = value; *s != '\0'; s++)
		hash = (hash << 5) + hash + *s;			// hash = (hash * 33) + *s. Το foo << 5 είναι γρηγορότερη εκδοχή του foo * 32.
    return hash;
}

uint hash_int(Pointer value) {
	return *(int*)value;
}

uint hash_pointer(Pointer value) {
	return (size_t)value;				// cast σε sizt_t, που έχει το ίδιο μήκος με έναν pointer
}