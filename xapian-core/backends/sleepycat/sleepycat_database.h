/* sleepy_database.h: C++ class definition for sleepycat access routines
 *
 * ----START-LICENCE----
 * -----END-LICENCE-----
 */

#ifndef _sleepy_database_h_
#define _sleepy_database_h_

#include "omassert.h"
#include "postlist.h"
#include "termlist.h"
#include "database.h"
#include <stdlib.h>

// Postlist - a list of documents indexed by a given term
class SleepyPostList : public virtual DBPostList {
    friend class SleepyDatabase;
    private:
	doccount pos;
	docid *data;

	doccount termfreq;

	SleepyPostList(docid *, doccount);
    public:
	~SleepyPostList();

	doccount   get_termfreq() const;// Number of docs indexed by this term

	docid      get_docid() const;   // Current docid
	weight     get_weight() const;  // Current weight
        PostList * next(weight);  // Move to next docid
        PostList * skip_to(docid, weight);  // Skip to next docid >= docid
	bool       at_end() const;      // True if we're off the end of the list
};



class SleepyDatabaseTermCache;
// Termlist - a list of terms indexing a given document
class SleepyTermList : public virtual DBTermList {
    friend class SleepyDatabase;
    private:
	termcount pos;
	termid *data;
	termcount terms;
	doccount dbsize;

	const SleepyDatabaseTermCache *termcache;

	SleepyTermList(const SleepyDatabaseTermCache *,
		       termid *,
		       termcount,
		       doccount);
    public:
	~SleepyTermList();
	termcount get_approx_size() const;

	ExpandBits get_weighting() const;  // Gets weight of current term
	const termname get_termname() const;  // Current term
	termcount get_wdf() const;  // Occurences of current term in doc
	doccount get_termfreq() const;  // Docs indexed by current term
	TermList * next();
	bool   at_end() const;
};



class SleepyDatabaseInternals;

class SleepyDatabaseTermCache {
    friend class SleepyDatabase;
    private:
	SleepyDatabaseInternals * internals;
	SleepyDatabaseTermCache(SleepyDatabaseInternals *i) : internals(i) {}
    public:
	termname term_id_to_name(termid) const;
	termid term_name_to_id(const termname &) const;
};

class SleepyDatabase : public virtual IRDatabase {
    friend class DatabaseBuilder;
    private:
	SleepyDatabaseInternals * internals;
	bool opened;

	SleepyDatabaseTermCache * termcache;

	void open(const DatabaseBuilderParams &);
	SleepyDatabase();
    public:
	~SleepyDatabase();

	doccount  get_doccount() const;
	doclength get_avlength() const;

	doccount get_termfreq(const termname &) const;
	bool term_exists(const termname &) const;

	DBPostList * open_post_list(const termname&, RSet *) const;
	DBTermList * open_term_list(docid) const;
	IRDocument * open_document(docid) const;

	void make_term(const termname &) {
	    throw OmError("DADatabase::make_term() not implemented");
	}
	docid make_doc(const docname &) {
	    throw OmError("DADatabase::make_doc() not implemented");
	}
	void make_posting(const termname &, unsigned int, unsigned int) {
	    throw OmError("DADatabase::make_posting() not implemented");
	}
};



///////////////////////////////////////////
// Inline definitions for SleepyPostList //
///////////////////////////////////////////

inline doccount
SleepyPostList::get_termfreq() const
{
    return termfreq;
}

inline docid
SleepyPostList::get_docid() const
{
    Assert(!at_end());
    Assert(pos != 0);
    return data[pos - 1];
}

inline PostList *
SleepyPostList::next(weight w_min)
{
    Assert(!at_end());
    pos ++;
    return NULL;
}

inline PostList *
SleepyPostList::skip_to(docid did, weight w_min)
{
    Assert(!at_end());
    if(pos == 0) pos++;
    while (!at_end() && data[pos - 1] < did) {
	PostList *ret = next(w_min);
	if (ret) return ret;
    }
    return NULL;
}

inline bool
SleepyPostList::at_end() const
{
    if(pos > termfreq) return true;
    return false;
}

///////////////////////////////////////////
// Inline definitions for SleepyTermList //
///////////////////////////////////////////

inline termcount
SleepyTermList::get_approx_size() const
{
    return terms;
}

inline ExpandBits
SleepyTermList::get_weighting() const {
    Assert(!at_end());
    Assert(pos != 0);
    Assert(wt != NULL);

    termcount wdf = 1; // FIXME - not yet stored in data structure
    doclength norm_len = 1.0; // FIXME - not yet stored in data structure

    return wt->get_bits(wdf, norm_len, SleepyTermList::get_termfreq(), dbsize);
}

inline const termname
SleepyTermList::get_termname() const
{
    Assert(!at_end());
    Assert(pos != 0);
    return termcache->term_id_to_name(data[pos]);
}

inline termcount
SleepyTermList::get_wdf() const
{
    Assert(!at_end());
    Assert(pos != 0);
    return 1;
}

inline doccount
SleepyTermList::get_termfreq() const
{
    Assert(!at_end());
    Assert(pos != 0);
    return 1;
}   

inline TermList *
SleepyTermList::next()
{
    Assert(!at_end());
    pos ++;
    return NULL;
}

inline bool
SleepyTermList::at_end() const
{
    if(pos > terms) return true;
    return false;
}

///////////////////////////////////////////
// Inline definitions for SleepyDatabase //
///////////////////////////////////////////

inline doccount
SleepyDatabase::get_doccount() const
{
    Assert(opened);
    return 1;
}

inline doclength
SleepyDatabase::get_avlength() const
{
    Assert(opened);
    return 1;
}

inline doccount
SleepyDatabase::get_termfreq(const termname &tname) const
{   
    PostList *pl = open_post_list(tname, NULL);
    doccount freq = 0;
    if(pl) freq = pl->get_termfreq();
    delete pl;
    return freq;
}

inline bool
SleepyDatabase::term_exists(const termname &tname) const
{
    if(termcache->term_name_to_id(tname)) return true;
    return false;
}

#endif /* _sleepy_database_h_ */
