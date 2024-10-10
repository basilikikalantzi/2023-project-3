/////////////////////////////////////////////////////////////////////////////
//
// Υλοποίηση του ADT Map μέσω υβριδικού Hash Table
//
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include "ADTMap.h"
#include "ADTVector.h"

// Κάθε θέση i θεωρείται γεινοτική με όλες τις θέσεις μέχρι και την i + NEIGHBOURS
#define NEIGHBOURS 3

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
struct map_node {
	Pointer key;		// Το κλειδί που χρησιμοποιείται για να hash-αρουμε
	Pointer value;  	// Η τιμή που αντισοιχίζεται στο παραπάνω κλειδί
	State state;		// Μεταβλητή για να μαρκάρουμε την κατάσταση των κόμβων (βλέπε διαγραφή)
};

// Δομή του Map (περιέχει όλες τις πληροφορίες που χρεαζόμαστε για το HashTable)
struct map {
	MapNode array;				// Ο πίνακας που θα χρησιμοποιήσουμε για το map (remember, φτιάχνουμε ένα hash table)
	Vector *chains;
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
	map->chains = malloc(map->capacity * sizeof(Vector));

	// Αρχικοποιούμε τους κόμβους που έχουμε σαν διαθέσιμους.
	for (int i = 0; i < map->capacity; i++){
		map->array[i].state = EMPTY;
		map->chains[i] = NULL;
	}	
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
	Vector *old_vector = map->chains;

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

	// Δημιουργούμε ένα μεγαλύτερο hash table και ένα μεγαλύτερο πίνακα από vector
	map->array = malloc(map->capacity * sizeof(struct map_node));
	map->chains = malloc(map->capacity * sizeof(Vector));
	for (int i = 0; i < map->capacity; i++){
		map->array[i].state = EMPTY;
		map->chains[i] = NULL;
	}
	
	// Τοποθετούμε ΜΟΝΟ τα entries που όντως περιέχουν ένα στοιχείο
	map->size = 0;
	for (int i = 0; i < old_capacity; i++){		// Διατρέχουμε το παλιό array και εισάγουμε τα στοιχεία στο νέο
		if (old_array[i].state == OCCUPIED){
			map_insert(map, old_array[i].key, old_array[i].value);
		}
		if(old_vector[i] != NULL){				// Αν στην αντίστοιχη θέση υπάρχει vector
			Vector vector = old_vector[i];
			for(int j=0 ; j<vector_size(vector); j++){	
				MapNode node = (MapNode)vector_get_at(vector, j);
				map_insert(map, node->key, node->value);		// Εισάγουμε τα στοιχεία του vector
				free(node);										// Διαγράφουμε τον κ΄όμβο
			}
			vector_destroy(old_vector[i]);	// Διαγράφουμε το vector
		}
	}

	//Αποδεσμεύουμε τον παλιό πίνακα ώστε να μήν έχουμε leaks
	free (old_vector);
	free(old_array);
}

// Βοηθητική συνάρτηση για εισαγωγή στον πίνακα από vector του ζευγαριού (key, item)
void insert_at_vector(Map map, Pointer key, Pointer value){
	
	uint pos = map->hash_function(key) % map->capacity;	 // Βρίσκουμε τη θέση που χασάρει το key 
	bool already_in_vector = false;
	MapNode new_node = malloc(sizeof(MapNode));			// Φτιάχνουμε τον κόμβο που θα εισάγουμε
	new_node->key = key;
	new_node->value = value;
	new_node->state = OCCUPIED;
	
	if(map->chains[pos] == NULL){						// Αν δεν θπάρχει vector στη θέση pos δημιουργούμε ένα
		map->chains[pos] = vector_create(0, NULL);
	}
	Vector vector = map->chains[pos];
	
	MapNode node = NULL;
	for(int i=0 ; i < vector_size(vector) ; i++){		// Διατρέχουμε το vector
		MapNode cur_node = (MapNode)vector_get_at(vector, i);
		if(cur_node != NULL){
			if (map->compare(cur_node->key, key) == 0) {	// Αν υπάρχει ήδη το στοιχείο με το key
				already_in_vector = true;
				node = cur_node;					// βρήκαμε το key, το ζευγάρι θα μπει αναγκαστικά εδώ (ακόμα και αν είχαμε προηγουμένως βρει DELETED θέση)
				break;											// και δε χρειάζεται να συνεχίζουμε την αναζήτηση.
			}
		}
	}

	// Σε αυτό το σημείο, το node είναι ο κόμβος στον οποίο θα γίνει εισαγωγή.
	if (already_in_vector) {
		// Αν αντικαθιστούμε παλιά key/value, τa κάνουμε destropy
		if (node->key != key && map->destroy_key != NULL)
			map->destroy_key(node->key);

		if (node->value != value && map->destroy_value != NULL)
			map->destroy_value(node->value);

		node->key = key;
		node->value = value;
		free(new_node); 	// Αν υπάρχει ήδη το στοιχείο με key τότε το new_node που δημιουργήσαμε είναι περιττό
	
	}
	else{
		// Νέο στοιχείο, αυξάνουμε τα συνολικά στοιχεία του map
		vector_insert_last(vector, new_node);
		map->size++;
	}
	// Αν με την νέα εισαγωγή ξεπερνάμε το μέγιστο load factor, πρέπει να κάνουμε rehash.
	// Στο load factor μετράμε και τα DELETED, γιατί και αυτά επηρρεάζουν τις αναζητήσεις.
	float load_factor = (float)(map->size) / map->capacity;
	if (load_factor > MAX_LOAD_FACTOR)
		rehash(map);
}


// Εισαγωγή στο hash table του ζευγαριού (key, item). Αν το key υπάρχει,
// ανανέωση του με ένα νέο value, και η συνάρτηση επιστρέφει true.
void map_insert(Map map, Pointer key, Pointer value) {
	// Σκανάρουμε το Hash Table μέχρι να βρούμε διαθέσιμη θέση για να τοποθετήσουμε το ζευγάρι,
	// ή μέχρι να βρούμε το κλειδί ώστε να το αντικαταστήσουμε.
	MapNode node = map_find_node(map, key);
	if(node != MAP_EOF){
		if (node->key != key && map->destroy_key != NULL)
			map->destroy_key(node->key);

		if (node->value != value && map->destroy_value != NULL)
			map->destroy_value(node->value);

		// Προσθήκη τιμών στον κόμβο
		node->key = key;
		node->value = value;
		return;
	}
	uint pos = map->hash_function(key) % map->capacity;		// Βρίσκουμε τη θέση που χασάρει το key
	bool in_vector = true;
	for (int i = 0;	i<= NEIGHBOURS;	i++) {					// Ψάχνουμε αν υπάρχει κενός γειτονικός κόμβος			
		
		if(map->array[pos].state == EMPTY){					
			in_vector = false;
			break;
		}

		pos = (pos + 1) % map->capacity;		// linear probing, γυρνώντας στην αρχή όταν φτάσουμε στη τέλος του πίνακα
	}
	
	if(in_vector){								//Αν το ζευγάρι (key , value) μπαίνει στο vector
		insert_at_vector(map, key, value);		// Κα΄λούμε τη βοηθητικη συνάρτηση για να το κάνει
		return;
	}
	// Διαφορετικά σημαίνει πως βρέθηκε κενός γειτονικός κόμβος
	node = &map->array[pos];
	map->size++;
	// Προσθήκη τιμών στον κόμβο
	node->state = OCCUPIED;
	node->key = key;
	node->value = value;

	// Αν με την νέα εισαγωγή ξεπερνάμε το μέγιστο load factor, πρέπει να κάνουμε rehash.
	// Στο load factor μετράμε και τα DELETED, γιατί και αυτά επηρρεάζουν τις αναζητήσεις.
	float load_factor = (float)(map->size) / map->capacity;
	if (load_factor > MAX_LOAD_FACTOR)
		rehash(map);
}


// Βοηθητική συνάρτηση για διαργραφή απο τον πίνακα με τα vector του κλειδιού με τιμή key
bool remove_from_vector(Map map, Pointer key) {
	
	uint pos = map->hash_function(key) % map->capacity;		// Βρίσκουμε τη θέση που χασάρει το key
	Vector vector = map->chains[pos];
	if(vector!=NULL){										// Αν υπάρχει το vector
		for(int i=0 ; i < vector_size(vector) ; i++){		// Διατρέχουμε το vector
			MapNode node = (MapNode)vector_get_at(vector, i);
			if(node != NULL){
				if (map->compare(node->key, key) == 0) {	// Αν βρέθει ο κόμβος με το ζευγάρι (key,value) που θα αφαιρέσουμε
					// destroy
					if (map->destroy_key != NULL)
						map->destroy_key(node->key);
					if (map->destroy_value != NULL)
						map->destroy_value(node->value);
					// Τον κάνουμε swap με τον τελευταίο κόμβο και τον αφαιρούμε από το vector
					MapNode last_node = (MapNode)vector_get_at(vector, vector_size(vector)-1);
					vector_set_at(vector, i, last_node);
					vector_remove_last(vector);
					map->size--;							// Μειώνουμε το μέγεθος του πίνακα
					return true;			
				}
			}
			
		}
	}
	return false;

}

// Διαργραφή απο το Hash Table του κλειδιού με τιμή key
bool map_remove(Map map, Pointer key) {
	
	uint pos = map->hash_function(key) % map->capacity;			// Βρίσκουμε τη θέση που χασάρει το key
	for(int i = 0; i <= NEIGHBOURS; i++){						// Ψάχνουμε αν το κλειδί key βρίσκεται σε γειτονικό κόμβο η στη θέση pos
		MapNode node = &map->array[pos];
		if(node->state == OCCUPIED && map->compare(node->key, key) == 0) {
			//destroy
			if (map->destroy_key != NULL)
				map->destroy_key(node->key);
			if (map->destroy_value != NULL)
				map->destroy_value(node->value);
			node->state = EMPTY;
			map->size--;
			return true;
			
		}
		pos = (pos + 1) % map->capacity;
	}
	//Διαφορετικά το key ή υπάρχει στο vector, οπότε καλούμε τη βοηθητική συνάρτηση για να κάνει remove, ή δεν υπάρχει
	return remove_from_vector(map, key);
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
		if (map->array[i].state == OCCUPIED) {		// Όταν βρούμε στοιχείο στο array διαγραφουμε το key και το value του
			if (map->destroy_key != NULL)
				map->destroy_key(map->array[i].key);
			if (map->destroy_value != NULL)
				map->destroy_value(map->array[i].value);
		}
		Vector vector = map->chains[i];
		if(vector!=NULL){							// Αν υπάρχει vector
			for(int j=0 ; j < vector_size(vector);  j++){
				MapNode node = (MapNode)vector_get_at(vector, 0); // Διαγράφει πάντα το πρωτο στοιχείο επειδή η remove from vector κάνει swap με το τελευταίο
				remove_from_vector(map, node->key);
			}
			vector_destroy(vector);					// Kαταστρέφουμε όλο το vector
		}
		
	}
	free(map->chains);
	free(map->array);
	free(map);
}

/////////////////////// Διάσχιση του map μέσω κόμβων ///////////////////////////

//Βοηθητική συνάρτηση που βρίσκει το πρώτο στοιχείο του πίνακα με τα Vector
MapNode first_in_vector(Map map) {
	// Είναι το πρώτο στοιχείο του πρώτου μη κενού vector που θα βρεθεί
	for (int i = 0; i < map->capacity; i++){
		if (map->chains[i]!=NULL && vector_size(map->chains[i]) != 0){
			return (MapNode)vector_get_at(map->chains[i], 0);
		}
	}
	// Αν δεν βρεθεί κανένα μη κενό vector τότε δεν υπάρχουν στοιχεία στον πίνακα 
	return MAP_EOF;
}

// Βρίσκει τον πρώτο κόμβο στο map
MapNode map_first(Map map) {
	//Ξεκινάμε την επανάληψή μας απο το 1ο στοιχείο, μέχρι να βρούμε κάτι όντως τοποθετημένο
	for (int i = 0; i < map->capacity; i++){
		if(map->array[i].state == OCCUPIED){
			return &map->array[i];
		}
	}
	// Αν δεν βρεθεί κανένα στοιχείο στο array τότε το πρ΄ώτο στοιχείο ,αν υπάρχει, είναι το πρώτο στοιχείο του vector
	return first_in_vector(map);
}


// Βοηθητική συνάρτηση που βρίσκει το επόμενο στοιχείο στον πίνακα με τα vector
MapNode next_in_vector(Map map, MapNode node) {
	
	uint pos = map->hash_function(node->key) % map->capacity;	// Βρίσκουμε τη θέση που χασάρει το key
	Vector vector = map->chains[pos];
	MapNode last_node = (MapNode)vector_get_at(vector, vector_size(vector) - 1);	// Ο τελευταίος κόμβος του vector
	// Αν ο κόμβος που παίρνουμε σσν όρισμα δεν είναι ο τελευταίος του vector
	if(map->compare(node->key, last_node->key) != 0){ 
		for(int i=0 ; i < vector_size(vector) ; i++){	// Διατρεχουμε το vector 
			MapNode cur_node = (MapNode)vector_get_at(vector, i);
			if (map->compare(cur_node->key, node->key) == 0) { // Βρίσκουμε τον κόμβο
				return vector_get_at(vector, i + 1);			// Και επιστρέφουμε τον επόμενο
			}
		}
	}
	// Διαφορετικά ο επόμενος κόμβος είναι ο πρώτος του επόμενου vector που θα βρούμε
	else{
		for (int i = pos + 1; i < map->capacity; i++){ // Διατρέχουμε το υπόλοιπο array
			if (map->chains[i]!=NULL && vector_size(map->chains[i]) != 0){  // Αν  βρεθεί επόμενο μη κενό vector
				return (MapNode)vector_get_at(map->chains[i], 0);			// Επιστρέφουμε το πρώτο στοιχείο του
			}
		}
	}
	// Αν δεν βρούμε άλλο vector δεν υπάρχει επόμενο στοιχείο
	return MAP_EOF;
}


// Βρίσκει τον επόμενο κόμβο
MapNode map_next(Map map, MapNode node){
	uint pos = map->hash_function(node->key) % map->capacity;	// Βρίσκουμε τη θέση που χασάρει το key του node
	bool in_array = false;
	for(int i = 0; i <= NEIGHBOURS; i++){		// Ψάχνουμε αν το κλειδί key βρίσκεται σε γειτονικό κόμβο ή στη θέση pos
		if(map->compare(node->key, map->array[pos].key) == 0){
			in_array = true;
			break;
		}
		pos = (pos + 1) % map->capacity;
	}
	if(in_array){							// Αν το node βρίσκεται στο array
		for (int i = pos + 1; i < map->capacity; i++){ // Διατρέχουμε το υπόλοιπο array μέχρι να βρουμε κάποιο στοιχείο
			if(map->array[i].state == OCCUPIED){
				return &map->array[i];	// Αν βρούμε το επιστρέφουμε
			}
		}
		return first_in_vector(map); // Αν δεν βρούμε τότε το επόμενο στοιχείο είναι το πρώτο στοιχείο του πίνακα με τα vector
	}
	return next_in_vector(map, node);	// Αν το node δεν βρίσκεται στο array τότε βρίσκεται στο vector η δεν υπάρχει καθόλου
}

Pointer map_node_key(Map map, MapNode node) {
	return node->key;
}

Pointer map_node_value(Map map, MapNode node) {
	return node->value;
}

// Βοηθητική συνάρτηση που βρίσκει(αν υπάρχει) το κλειδί με τιμή key στο vector
MapNode search_at_vector(Map map, Pointer key) {
	uint pos = map->hash_function(key) % map->capacity;		// Βρίσκουμε τη θέση που χασάρει το key
	Vector vector = map->chains[pos];
	if(vector != NULL){										// Αν υπάρχει το vector
		for(int i=0 ; i < vector_size(vector) ; i++){		// Διατρέχουμε το vector
			MapNode node = (MapNode)vector_get_at(vector, i);
			if (map->compare(node->key, key) == 0) {		// Αν βρέθει ο κόμβος με το ζευγάρι (key,value)
				return node;								// Τον επιστρέφουμε
			}
		}
	}
	return MAP_EOF;
}



MapNode map_find_node(Map map, Pointer key) {
	uint pos = 	map->hash_function(key) % map->capacity;
	for (int i = 0;	i<= NEIGHBOURS;	i++) {	// Ψάχνουμε αν το κλειδί key βρίσκεται σε γειτονικό κόμβο η στη θέση pos

		// Μόνο σε OCCUPIED θέσεις, ελέγχουμε αν το key είναι εδώ
		if (map->array[pos].state == OCCUPIED && map->compare(map->array[pos].key, key) == 0){
			return &map->array[pos];
		}
		pos = (pos + 1) % map->capacity;        
	}
	// Διαφορετικά δεν υπαρχει στο array και ψάχνουμε αν υπάρχει στο vector
	return search_at_vector(map, key);
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