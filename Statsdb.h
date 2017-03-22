//
// Author: Matt Wells
//
// Rdb to store the stats for each host

#ifndef GB_STATSDB_H
#define GB_STATSDB_H

#include "Rdb.h"
#include "RdbList.h"
#include "Msg1.h"
#include "Msg5.h"
#include "SafeBuf.h"
#include "HashTableX.h"
#include "fctypes.h"

#define ST_BUF_SIZE (32*1024)

class Statsdb {

 public:
	Statsdb();
	//~Statsdb();

	// reset m_rdb
	void reset() { m_rdb.reset(); }


	// initialize m_rdb
	bool init( );

	void addDocsIndexed ( ) ;

	void enable() { m_disabled = false; }
	void disable() { m_disabled = true; }

	// . returns false and sets g_errno on error, true otherwise
	// . we only add the stat to our local statsdb rdb, but because
	//   we might be dumping statsdb to disk or something it is possible
	//   we get an ETRYAGAIN error, so we try to accumulate stats in a
	//   local buffer in that case
	bool addStat ( const char    *label    ,
		       int64_t   t1       ,
		       int64_t   t2       ,
		       float       value    , // y-value really, "numBytes"
		       int32_t        parmHash = 0   ,
		       float       oldVal   = 0.0 ,
		       float       newVal   = 0.0 ,
		       int32_t        userId32 = 0 );
	
	bool makeGIF ( int32_t t1Arg , 
		       int32_t t2Arg ,
		       int32_t samples ,
		       // fill in the map key in html into "sb2"
		       SafeBuf *sb2 ,
		       void *state ,
		       void (* callback) (void *state) ,
		       int32_t userId32 = 0 ) ;

	Rdb *getRdb() { return &m_rdb; }

	static void gotListWrapper(void *state, RdbList *list, Msg5 *msg5);

	// the graphing window. now a bunch of absolute divs in html
	SafeBuf m_gw;
	HashTableX m_dupTable;

private:
	class Label *getLabel(int32_t graphHash);

	char *plotGraph(char *pstart, char *pend, int32_t graphHash, SafeBuf &gw, int32_t zoff);
	void drawHR(float z, float ymin, float ymax, SafeBuf &gw, class Label *label, float zoff, int32_t color);
	void drawLine3(SafeBuf &sb, int32_t x1, int32_t x2, int32_t fy1, int32_t color, int32_t width);

	class StatState *getStatState(int32_t us);

	bool addPointsFromList(class Label *label);
	bool addPoint(class StatKey *sk, class StatData *sd, class StatState *ss, class Label *label);
	bool addPoint(int32_t x, float y, int32_t graphHash, float weight, class StatState *ss);
	bool addEventPointsFromList();
	bool addEventPoint(int32_t t1, int32_t parmHash, float oldVal, float newVal, int32_t thickness);

	bool gifLoop();
	bool processList();

	// so Process.cpp can turn it off when saving so we do not record
	// disk writes/reads
	bool m_disabled;

	Rdb 	  m_rdb;
	RdbList   m_list;

	SafeBuf m_sb0;
	SafeBuf m_sb1;

	SafeBuf m_sb3;

	SafeBuf *m_sb2;

	HashTableX m_ht0;
	HashTableX m_ht3;

	HashTableX m_labelTable;

	int32_t m_niceness;

	// pixel boundaries
	int32_t m_bx;
	int32_t m_by;

	// # of samples in moving avg
	int32_t m_samples;

	void *m_state;
	void (*m_callback )(void *state);

	// some flag
	bool m_done;

	key96_t m_startKey;
	key96_t m_endKey;

	Msg5 m_msg5;

	// time window to graph
	int32_t m_t1;
	int32_t m_t2;

	bool m_init;
};

extern class Statsdb g_statsdb;

// . we now take a snapshot every one second and accumulate all stats
//   ending a Stat::m_time1 for that second into this Stat
// . so basically Statsdb::addStat() accumulates stats for each second
//   based on the label hash
class StatKey {
 public:
	// these two vars make up the key96_t!
	uint32_t m_zero;
	uint32_t m_labelHash;
	// force to 32-bit even though time_t is 64-bit on 64-bit systems
	int32_t m_time1;
} __attribute__((packed, aligned(4)));

class StatData {
 public:
	float      m_totalOps;
	float      m_totalQuantity; // aka "m_oldVal"
	float      m_totalTime; // in SECONDS!

	float      m_newVal;

	// set the m_key members based on the data members
	float     getOldVal () { return m_totalQuantity; } // aliased
	float     getNewVal () { return m_newVal ; }
	bool      isEvent       () { return almostEqualFloat(m_totalOps, 0.0); }
} __attribute__((packed, aligned(4)));

#endif // GB_STATSDB_H
