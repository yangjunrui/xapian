/* apitest.cc: tests the OpenMuscat API
 *
 * ----START-LICENCE----
 * Copyright 1999,2000 BrightStation PLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 * -----END-LICENCE-----
 */

#include "config.h"
#include <iostream>
#include <string>
#include <memory>
#include <map>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <vector>

using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::ostream_iterator;
using std::map;
using std::auto_ptr;
using std::max;
using std::ostream;

#include "om/om.h"
#include "testsuite.h"
#include "testutils.h"
#include "textfile_indexer.h"
#include "../indexer/index_utils.h"
#include "backendmanager.h"
#include "utils.h"

OmDatabase
make_dbgrp(OmDatabase * db1 = 0,
	   OmDatabase * db2 = 0,
	   OmDatabase * db3 = 0,
	   OmDatabase * db4 = 0)
{
    OmDatabase result;

    if(db1 != 0) result.add_database(*db1);
    if(db2 != 0) result.add_database(*db2);
    if(db3 != 0) result.add_database(*db3);
    if(db4 != 0) result.add_database(*db4);

    return result;
}

static BackendManager backendmanager;

OmDatabase get_database(const string &dbname, const string &dbname2 = "") {
    return backendmanager.get_database(dbname, dbname2);
}

// #######################################################################
// # Tests start here

// always succeeds
static bool test_trivial()
{
    return true;
}

#if 0
// always fails (for testing the framework)
static bool test_alwaysfail()
{
    return false;
}
#endif

// tests that the backend doesn't return zero docids
static bool test_zerodocid()
{
    // open the database (in this case a simple text file
    // we prepared earlier)

    OmDatabase mydb(get_database("apitest_onedoc"));

    OmEnquire enquire(make_dbgrp(&mydb));

    // make a simple query, with one word in it - "word".
    OmQuery myquery("word");
    enquire.set_query(myquery);

    // retrieve the top ten results (we only expect one)
    OmMSet mymset = enquire.get_mset(0, 10);

    // We've done the query, now check that the result is what
    // we expect (1 document, with non-zero docid)
    if (mymset.items.size() != 1)
	FAIL_TEST("Expected 1 item, got " << mymset.items.size());

    if (mymset.items[0].did == 0)
	FAIL_TEST("A query on a database returned a zero docid");

    return true;
}

OmDatabase get_simple_database()
{
    return OmDatabase(get_database("apitest_simpledata"));
}

void init_simple_enquire(OmEnquire &enq, const OmQuery &query = OmQuery("thi"))
{
    enq.set_query(query);
}

OmMSet do_get_simple_query_mset(OmQuery query, int maxitems = 10, int first = 0)
{
    // open the database (in this case a simple text file
    // we prepared earlier)
    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire, query);

    // retrieve the top results
    return enquire.get_mset(first, maxitems);
}

// tests the document count for a simple query
static bool test_simplequery1()
{
    OmMSet mymset = do_get_simple_query_mset(OmQuery("word"));
    TEST_EQUAL(mymset.items.size(), 2);
    return true;
}

// tests for the right documents and weights returned with simple query
static bool test_simplequery2()
{
    OmMSet mymset = do_get_simple_query_mset(OmQuery("word"));

    // We've done the query, now check that the result is what
    // we expect (documents 2 and 4)
    mset_expect_order(mymset, 2, 4);

    // Check the weights
    weights_are_equal_enough(mymset.items[0].wt, 0.661095);
    weights_are_equal_enough(mymset.items[1].wt, 0.56982);

    return true;
}

// tests for the right document count for another simple query
static bool test_simplequery3()
{
    // The search is for "thi" rather than "this" because
    // the index will have stemmed versions of the terms.
    OmMSet mymset = do_get_simple_query_mset(OmQuery("thi"));

    // Check that 6 documents were returned.
    TEST_EQUAL(mymset.items.size(), 6);

    return true;
}

// tests a query accross multiple databases
static bool test_multidb1()
{
    bool success = true;

    OmDatabase mydb1(get_database("apitest_simpledata", "apitest_simpledata2"));
    OmEnquire enquire1(make_dbgrp(&mydb1));

    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    // make a simple query, with one word in it - "word".
    OmQuery myquery("word");
    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    // retrieve the top ten results from each method of accessing
    // multiple text files
    OmMSet mymset1 = enquire1.get_mset(0, 10);
    OmMSet mymset2 = enquire2.get_mset(0, 10);

    if (mymset1.items.size() != mymset2.items.size()) {
	if (verbose) {
	    cout << "Match sets are of different size: "
		    << mymset1.items.size() << "vs." << mymset2.items.size()
		    << endl;
	}
	success = false;
    } else if (!mset_range_is_same_weights(mymset1, 0,
					   mymset2, 0,
					   mymset1.items.size())) {
	if (verbose) {
	    cout << "Match sets don't compare equal:" << endl;
	    cout << mymset1 << "vs." << endl << mymset2 << endl;
	}
	success = false;
    }
    return success;
}

// tests a query accross multiple databases with terms only
// in one of the two databases
static bool test_multidb2()
{
    bool success = true;

    OmDatabase mydb1(get_database("apitest_simpledata",
				  "apitest_simpledata2"));
    OmEnquire enquire1(make_dbgrp(&mydb1));

    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    // make a simple query
    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery("inmemory"),
		    OmQuery("word"));
    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    // retrieve the top ten results from each method of accessing
    // multiple text files
    OmMSet mymset1 = enquire1.get_mset(0, 10);

    OmMSet mymset2 = enquire2.get_mset(0, 10);

    if (mymset1.items.size() != mymset2.items.size()) {
	if (verbose) {
	    cout << "Match sets are of different size: "
		    << mymset1.items.size() << "vs." << mymset2.items.size()
		    << endl;
	}
	success = false;
    } else if (!mset_range_is_same_weights(mymset1, 0,
					   mymset2, 0,
					   mymset1.items.size())) {
	if (verbose) {
	    cout << "Match sets don't compare equal:" << endl;
	    cout << mymset1 << "vs." << endl << mymset2 << endl;
	}
	success = false;
    }
    return success;
}

// tests that changing a query object after calling set_query()
// doesn't make any difference to get_mset().
static bool test_changequery1()
{
    // The search is for "thi" rather than "this" because
    // the index will have stemmed versions of the terms.
    // open the database (in this case a simple text file
    // we prepared earlier)
    OmEnquire enquire(get_simple_database());

    OmQuery myquery("thi");
    // make a simple query
    enquire.set_query(myquery);

    // retrieve the top ten results
    OmMSet mset1 = enquire.get_mset(0, 10);

    myquery = OmQuery("foo");
    OmMSet mset2 = enquire.get_mset(0, 10);

    // verify that both msets are identical
    TEST_EQUAL(mset1, mset2);
    return true;
}

// tests that a null query throws an exception
static bool test_nullquery1()
{
    TEST_EXCEPTION(OmInvalidArgumentError,
		   OmMSet mymset = do_get_simple_query_mset(OmQuery()));
    return true;
}

// tests that when specifiying maxitems to get_mset, no more than
// that are returned.
static bool test_msetmaxitems1()
{
    OmMSet mymset = do_get_simple_query_mset(OmQuery("thi"), 1);
    TEST_EQUAL(mymset.items.size(), 1);
    return true;
}

// tests that when specifiying maxitems to get_eset, no more than
// that are returned.
static bool test_expandmaxitems1()
{
    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmMSet mymset = enquire.get_mset(0, 10);
    TEST(mymset.items.size() >= 2);

    OmRSet myrset;
    myrset.add_document(mymset.items[0].did);
    myrset.add_document(mymset.items[1].did);

    OmESet myeset = enquire.get_eset(1, myrset);
    TEST_EQUAL(myeset.items.size(), 1);

    return true;
}

// tests that a pure boolean query has all weights set to 1
static bool test_boolquery1()
{
    OmQuery myboolquery(OmQuery(OmQuery::OP_FILTER,
				OmQuery(),
				OmQuery("thi")));
    OmMSet mymset = do_get_simple_query_mset(myboolquery);

    TEST_NOT_EQUAL(mymset.items.size(), 0);
    TEST_EQUAL(mymset.max_possible, 0);
    for (unsigned int i = 0; i<mymset.items.size(); ++i) {
	TEST_EQUAL(mymset.items[i].wt, 0);
    }

    return true;
}

// tests that get_mset() specifying "first" works as expected
static bool test_msetfirst1()
{
    OmMSet mymset1 = do_get_simple_query_mset(OmQuery("thi"), 6, 0);
    OmMSet mymset2 = do_get_simple_query_mset(OmQuery("thi"), 3, 3);

    TEST(mset_range_is_same(mymset1, 3, mymset2, 0, 3));
    return true;
}

// tests the converting-to-percent functions
static bool test_topercent1()
{
    bool success = true;
    OmMSet mymset = do_get_simple_query_mset(OmQuery("thi"), 20, 0);

    int last_pct = 101;
    for (unsigned i=0; i<mymset.items.size(); ++i) {
	int pct = mymset.convert_to_percent(mymset.items[i]);
	if (pct != mymset.convert_to_percent(mymset.items[i].wt)) {
	    success = false;
	    if (verbose) {
		cout << "convert_to_%(msetitem) != convert_to_%(wt)" << endl;
	    }
	} else if ((pct < 0) || (pct > 100)) {
	    success = false;
	    if (verbose) {
	        cout << "percentage out of range: " << pct << endl;
	    }
	} else if (pct > last_pct) {
	    success = false;
	    if (verbose) {
	        cout << "percentage increased over mset" << endl;
	    }
	}
	last_pct = pct;
    }
    return success;
}

class myExpandFunctor : public OmExpandDecider {
    public:
	int operator()(const om_termname & tname) const {
	    unsigned long sum = 0;
	    for (om_termname::const_iterator i=tname.begin(); i!=tname.end(); ++i) {
		sum += *i;
	    }
//	    if (verbose) {
//		cout << tname << "==> " << sum << endl;
//	    }
	    return (sum % 2) == 0;
	}
};

// tests the expand decision functor
static bool test_expandfunctor1()
{
    bool success = true;

    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmMSet mymset = enquire.get_mset(0, 10);
    if (mymset.items.size() < 2) return false;

    OmRSet myrset;
    myrset.add_document(mymset.items[0].did);
    myrset.add_document(mymset.items[1].did);

    myExpandFunctor myfunctor;

    OmESet myeset_orig = enquire.get_eset(1000, myrset);
    unsigned int neweset_size = 0;
    for (unsigned int i=0; i<myeset_orig.items.size(); ++i) {
        if (myfunctor(myeset_orig.items[i].tname)) neweset_size++;
    }
    OmESet myeset = enquire.get_eset(neweset_size, myrset, 0, &myfunctor);

    // Compare myeset with the hand-filtered version of myeset_orig.
    if (verbose) {
	cout << "orig_eset: ";
	copy(myeset_orig.items.begin(), myeset_orig.items.end(),
	     ostream_iterator<OmESetItem>(cout, " "));
	cout << endl;

	cout << "new_eset: ";
	copy(myeset.items.begin(), myeset.items.end(),
	     ostream_iterator<OmESetItem>(cout, " "));
	cout << endl;
    }
    vector<OmESetItem>::const_iterator orig,filt;
    for (orig=myeset_orig.items.begin(), filt=myeset.items.begin();
         orig!=myeset_orig.items.end() && filt!=myeset.items.end();
	 ++orig, ++filt) {
	// skip over items that shouldn't be in myeset
	while (orig != myeset_orig.items.end() && !myfunctor(orig->tname)) {
	    ++orig;
	}

	if ((orig->tname != filt->tname) ||
	    (orig->wt != filt->wt)) {
	    success = false;
	    if (verbose) {
	        cout << "Mismatch in items "
	             << orig->tname
		     << " vs. "
		     << filt->tname
		     << " after filtering" << endl;
	    }
	    break;
	}
    }

    while (orig != myeset_orig.items.end() && !myfunctor(orig->tname)) {
	++orig;
    }

    if (orig != myeset_orig.items.end()) {
	success = false;
	if (verbose) {
	    cout << "Extra items in the non-filtered eset:";
            copy(orig,
		 const_cast<const OmESet &>(myeset_orig).items.end(),
		 ostream_iterator<OmESetItem>(cout, " "));
	    cout << endl;
	}
    } else if (filt != myeset.items.end()) {
        success = false;
	if (verbose) {
	    cout << "Extra items in the filtered eset." << endl;
	}
    }

    return success;
}

class myMatchDecider : public OmMatchDecider {
    public:
        int operator()(const OmDocument &doc) const {
	    // Note that this is not recommended usage of get_data()
	    return strncmp(doc.get_data().value.c_str(), "This is", 7) == 0;
	}
};

// tests the match decision functor
static bool test_matchfunctor1()
{
    // FIXME: check that the functor works both ways.
    bool success = true;

    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    myMatchDecider myfunctor;

    OmMSet mymset = enquire.get_mset(0, 100, 0, 0, &myfunctor);

    for (unsigned int i=0; i<mymset.items.size(); ++i) {
	const OmDocument doc(enquire.get_doc(mymset.items[i]));
        if (!myfunctor(doc)) {
	    success = false;
	    break;
	}
    }

    return success;
}

void print_mset_percentages(const OmMSet &mset)
{
    for (unsigned i=0; i<mset.items.size(); ++i) {
        cout << " ";
	cout << mset.convert_to_percent(mset.items[i]);
    }
}

// tests the percent cutoff option
static bool test_pctcutoff1()
{
    bool success = true;

    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmMSet mymset1 = enquire.get_mset(0, 100);

    if (verbose) {
      cout << "Original mset pcts:";
      print_mset_percentages(mymset1);
      cout << endl;
    }

    unsigned int num_items = 0;
    int my_pct = 100;
    int changes = 0;
    for (unsigned int i=0; i<mymset1.items.size(); ++i) {
        int new_pct = mymset1.convert_to_percent(mymset1.items[i]);
        if (new_pct != my_pct) {
	    changes++;
	    if (changes <= 3) {
	        num_items = i;
		my_pct = new_pct;
	    }
	}
    }

    if (changes <= 3) {
	success = false;
        if (verbose) {
	    cout << "MSet not varied enough to test" << endl;
	}
    }
    if (verbose) {
        cout << "Cutoff percent: " << my_pct << endl;
    }

    OmSettings mymopt;
    mymopt.set("match_percent_cutoff", my_pct);
    OmMSet mymset2 = enquire.get_mset(0, 100, NULL, &mymopt);

    if (verbose) {
        cout << "Percentages after cutoff:";
	print_mset_percentages(mymset2);
        cout << endl;
    }

    if (mymset2.items.size() < num_items) {
        success = false;
	if (verbose) {
	    cout << "Match with % cutoff lost too many items" << endl;
	}
    } else if (mymset2.items.size() > num_items) {
        for (unsigned int i=num_items; i<mymset2.items.size(); ++i) {
	    if (mymset2.convert_to_percent(mymset2.items[i]) != my_pct) {
	        success = false;
		if (verbose) {
		    cout << "Match with % cutoff returned too many items"
			 << endl;
		}
		break;
	    }
	}
    }

    return success;
}

// tests the allow query terms expand option
static bool test_allowqterms1()
{
    bool success = true;

    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmMSet mymset = enquire.get_mset(0, 10);
    if (mymset.items.size() < 2) return false;

    OmRSet myrset;
    myrset.add_document(mymset.items[0].did);
    myrset.add_document(mymset.items[1].did);

    OmSettings eopt;
    eopt.set("expand_use_query_terms", false);

    OmESet myeset = enquire.get_eset(1000, myrset, &eopt);

    for (unsigned i=0; i<myeset.items.size(); ++i) {
        if (myeset.items[i].tname == "thi") {
	    success = false;
	    if (verbose) {
	        cout << "Found query term `"
		     << myeset.items[i].tname
		     << "' in expand set" << endl;
	    }
	    break;
	}
    }

    return success;
}

// tests that the MSet max_attained works
static bool test_maxattain1()
{
    bool success = true;

    OmMSet mymset = do_get_simple_query_mset(OmQuery("thi"), 100, 0);

    om_weight mymax = 0;
    for (unsigned i=0; i<mymset.items.size(); ++i) {
        if (mymset.items[i].wt > mymax) {
	    mymax = mymset.items[i].wt;
	}
    }
    if (mymax != mymset.max_attained) {
        success = false;
	if (verbose) {
	    cout << "Max weight in MSet is " << mymax
	         << ", max_attained = " << mymset.max_attained << endl;
        }
    }

    return success;
}

// tests the collapse-on-key
static bool test_collapsekey1()
{
    bool success = true;

    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmSettings mymopt;

    OmMSet mymset1 = enquire.get_mset(0, 100, 0, &mymopt);
    om_doccount mymsize1 = mymset1.items.size();

    for (int key_no = 1; key_no<7; ++key_no) {
	mymopt.set("match_collapse_key", key_no);
	OmMSet mymset = enquire.get_mset(0, 100, 0, &mymopt);

	if(mymsize1 <= mymset.items.size()) {
	    success = false;
	    if (verbose) {
		cout << "Had no fewer items when performing collapse: don't know whether it worked." << endl;
	    }
	}

        map<string, om_docid> keys;
	for (vector<OmMSetItem>::const_iterator i=mymset.items.begin();
	     i != mymset.items.end();
	     ++i) {
	    OmKey key = enquire.get_doc(*i).get_key(key_no);
	    if (key.value != i->collapse_key.value) {
		success = false;
		if (verbose) {
		    cout << "Expected key value was not found in MSetItem: "
			 << "expected `" << key.value
			 << "' found `" << i->collapse_key.value
			 << "'" << endl;

		}
	    }
	    if (keys[key.value] != 0 && key.value != "") {
	        success = false;
		if (verbose) {
		    cout << "docids " << keys[key.value]
		         << " and " << i->did
			 << " both found in MSet with key `" << key.value
			 << "'" << endl;
		}
		break;
	    } else {
	        keys[key.value] = i->did;
	    }
	}
	// don't bother continuing if we've already failed.
	if (!success) break;
    }

    return success;
}

// tests a reversed boolean query
static bool test_reversebool1()
{
    OmEnquire enquire(get_simple_database());
    OmQuery query("thi");
    query.set_bool(true);
    init_simple_enquire(enquire, query);

    OmSettings mymopt;
    OmMSet mymset1 = enquire.get_mset(0, 100, 0, &mymopt);
    mymopt.set("match_sort_forward", true);
    OmMSet mymset2 = enquire.get_mset(0, 100, 0, &mymopt);
    mymopt.set("match_sort_forward", false);
    OmMSet mymset3 = enquire.get_mset(0, 100, 0, &mymopt);

    if(mymset1.items.size() == 0) {
	if (verbose) cout << "Mset was empty" << endl;
	return false;
    }

    // mymset1 and mymset2 should be identical
    if(mymset1.items.size() != mymset2.items.size()) {
	if (verbose) {
	    cout << "mymset1 and mymset2 were of different sizes (" <<
		    mymset1.items.size() << " and " <<
		    mymset2.items.size() << ")" << endl;
	}
	return false;
    }

    {
	vector<OmMSetItem>::const_iterator i;
	vector<OmMSetItem>::const_iterator j;
	for (i = mymset1.items.begin(), j = mymset2.items.begin();
	     i != mymset1.items.end(), j != mymset2.items.end();
	     ++i, j++) {
	    if(i->did != j->did) {
		if (verbose) {
		    cout << "Setting match_sort_forward=true was not"
			    "same as default." << endl;
		    cout << "docids " << i->did << " and " << j->did <<
			    " should have been the same" << endl;
		}
		return false;
	    }
	}
    }

    // mymset1 and mymset3 should be same but reversed
    if(mymset1.items.size() != mymset3.items.size()) {
	if (verbose) {
	    cout << "mymset1 and mymset2 were of different sizes (" <<
		    mymset1.items.size() << " and " <<
		    mymset2.items.size() << ")" << endl;
	}
	return false;
    }

    {
	vector<OmMSetItem>::const_iterator i;
	vector<OmMSetItem>::reverse_iterator j;
	for (i = mymset1.items.begin(),
	     j = mymset3.items.rbegin();
	     i != mymset1.items.end();
	     ++i, j++) {
	    if(i->did != j->did) {
		if (verbose) {
		    cout << "Setting match_sort_forward=false "
			    "did not reverse results." << endl;
		    cout << "docids " << i->did << " and " << j->did <<
			    " should have been the same" << endl;
		}
		return false;
	    }
	}
    }

    return true;
}

// tests a reversed boolean query, where the full mset isn't returned
static bool test_reversebool2()
{
    OmEnquire enquire(get_simple_database());
    OmQuery query("thi");
    query.set_bool(true);
    init_simple_enquire(enquire, query);

    OmSettings mymopt;
    OmMSet mymset1 = enquire.get_mset(0, 100, 0, &mymopt);

    if(mymset1.items.size() == 0) {
	if (verbose) cout << "Mset was empty" << endl;
	return false;
    }
    if(mymset1.items.size() == 1) {
	if (verbose) cout << "Mset was too small to test properly" << endl;
	return false;
    }

    mymopt.set("match_sort_forward", true);
    om_doccount msize = mymset1.items.size() / 2;
    OmMSet mymset2 = enquire.get_mset(0, msize, 0, &mymopt);
    mymopt.set("match_sort_forward", false);
    OmMSet mymset3 = enquire.get_mset(0, msize, 0, &mymopt);

    // mymset2 should be first msize items of mymset1
    if(msize != mymset2.items.size()) return false;
    {
	vector<OmMSetItem>::const_iterator i;
	vector<OmMSetItem>::const_iterator j;
	for (i = mymset1.items.begin(), j = mymset2.items.begin();
	     i != mymset1.items.end(), j != mymset2.items.end();
	     ++i, j++) {
	    if(i->did != j->did) {
		if (verbose) {
		    cout << "Setting match_sort_forward=true was not"
			    "same as default." << endl;
		    cout << "docids " << i->did << " and " << j->did <<
			    " should have been the same" << endl;
		}
		return false;
	    }
	}
    }

    // mymset3 should be last msize items of mymset1, in reverse order
    if(msize != mymset3.items.size()) return false;
    {
	vector<OmMSetItem>::reverse_iterator i;
	vector<OmMSetItem>::const_iterator j;
	for (i = mymset1.items.rbegin(),
	     j = mymset3.items.begin();
	     j != mymset3.items.end();
	     ++i, j++) {
	    if(i->did != j->did) {
		if (verbose) {
		    cout << "Setting match_sort_forward=false "
			    "did not reverse results." << endl;
		    cout << "docids " << i->did << " and " << j->did <<
			    " should have been the same" << endl;
		}
		return false;
	    }
	}
    }

    return true;
}

// tests that get_query_terms() returns the terms in the right order
static bool test_getqterms1()
{
    bool success;

    static const char *answers[4] = {
	"one",
	"two",
	"three",
	"four"
    };

    OmQuery myquery(OmQuery::OP_OR,
	    OmQuery(OmQuery::OP_AND,
		    OmQuery("one", 1, 1),
		    OmQuery("three", 1, 3)),
	    OmQuery(OmQuery::OP_OR,
		    OmQuery("four", 1, 4),
		    OmQuery("two", 1, 2)));

    om_termname_list terms = myquery.get_terms();

    om_termname_list answers_list;
    for (unsigned int i=0; i<(sizeof(answers) / sizeof(answers[0])); ++i) {
        answers_list.push_back(answers[i]);
    }
    success = (terms == answers_list);
    if (verbose && !success) {
	cout << "Terms returned in incorrect order: ";
	copy(terms.begin(),
	     terms.end(),
	     ostream_iterator<om_termname>(cout, " "));
	cout << endl << "Expected: one two three four" << endl;
    }

    return success;
}

// tests that get_matching_terms() returns the terms in the right order
static bool test_getmterms1()
{
    bool success = true;

    static const char *answers[4] = {
	"one",
	"two",
	"three",
	"four"
    };

    OmDatabase mydb(get_database("apitest_termorder"));
    OmEnquire enquire(make_dbgrp(&mydb));

    OmQuery myquery(OmQuery::OP_OR,
	    OmQuery(OmQuery::OP_AND,
		    OmQuery("one", 1, 1),
		    OmQuery("three", 1, 3)),
	    OmQuery(OmQuery::OP_OR,
		    OmQuery("four", 1, 4),
		    OmQuery("two", 1, 2)));

    enquire.set_query(myquery);

    OmMSet mymset = enquire.get_mset(0, 10);

    if (mymset.items.size() != 1) {
	success = false;
	if (verbose) {
	    cout << "Expected one match, but got " << mymset.items.size()
		 << "!" << endl;
	}
    } else {
	om_termname_list mterms = enquire.get_matching_terms(mymset.items[0]);
        om_termname_list answers_list;
	for (unsigned int i=0; i<(sizeof(answers) / sizeof(answers[0])); ++i) {
		answers_list.push_back(answers[i]);
	}
	if (mterms != answers_list) {
	    success = false;
	    if (verbose) {
		cout << "Terms returned in incorrect order: ";
		copy(mterms.begin(),
		     mterms.end(),
		     ostream_iterator<om_termname>(cout, " "));
		cout << endl << "Expected: one two three four" << endl;
	    }
	}

    }

    return success;
}

// tests that building a query with boolean sub-queries throws an exception.
static bool test_boolsubq1()
{
    OmQuery mybool("foo");
    mybool.set_bool(true);

    TEST_EXCEPTION(OmInvalidArgumentError,
		   OmQuery query(OmQuery::OP_OR, OmQuery("bar"), mybool));
    return true;
}

// tests that specifying a nonexistent input file throws an exception.
static bool test_absentfile1()
{
    TEST_EXCEPTION(OmOpeningError,
		   OmDatabase mydb(get_database("/this_does_not_exist"));
		   OmEnquire enquire(make_dbgrp(&mydb));
		   
		   OmQuery myquery("cheese");
		   enquire.set_query(myquery);
		   
		   OmMSet mymset = enquire.get_mset(0, 10);)
    return true;
}

// tests that query lengths are calculated correctly
static bool test_querylen1()
{
    // test that a null query has length 0
    bool success = (OmQuery().get_length()) == 0;

    return success;
}

// tests that query lengths are calculated correctly
static bool test_querylen2()
{
    // test that a simple query has the right length
    bool success = true;

    OmQuery myquery;

    myquery = OmQuery(OmQuery::OP_OR,
		      OmQuery("foo"),
		      OmQuery("bar"));
    myquery = OmQuery(OmQuery::OP_AND,
		      myquery,
		      OmQuery(OmQuery::OP_OR,
			      OmQuery("wibble"),
			      OmQuery("spoon")));

    if (myquery.get_length() != 4) {
	success = false;
	if (verbose) {
	    cout << "Query had length "
		 << myquery.get_length()
		 << ", expected 4" << endl;
	}
    }

    return success;
}

// tests that query lengths are calculated correctly
static bool test_querylen3()
{
    bool success = true;

    // test with an even bigger and strange query

    om_termname terms[3] = {
	"foo",
	"bar",
	"baz"
    };
    OmQuery queries[3] = {
	OmQuery("wibble"),
	OmQuery("wobble"),
	OmQuery(OmQuery::OP_OR, std::string("jelly"), std::string("belly"))
    };

    OmQuery myquery;
    vector<om_termname> v1(terms, terms+3);
    vector<OmQuery> v2(queries, queries+3);
    vector<OmQuery *> v3;

    auto_ptr<OmQuery> dynquery1(new OmQuery(OmQuery::OP_AND,
					    std::string("ball"),
					    std::string("club")));
    auto_ptr<OmQuery> dynquery2(new OmQuery("ring"));
    v3.push_back(dynquery1.get());
    v3.push_back(dynquery2.get());

    OmQuery myq1 = OmQuery(OmQuery::OP_AND, v1.begin(), v1.end());
    if (myq1.get_length() != 3) {
	success = false;
	if (verbose) {
	    cout << "Query myq1 length is "
		    << myq1.get_length()
		    << ", expected 3.  Description: "
		    << myq1.get_description() << endl;
	}
    }

    OmQuery myq2_1 = OmQuery(OmQuery::OP_OR, v2.begin(), v2.end());
    if (myq2_1.get_length() != 4) {
	success = false;
	if (verbose) {
	    cout << "Query myq2_1 length is "
		    << myq2_1.get_length()
		    << ", expected 4.  Description: "
		    << myq2_1.get_description() << endl;
	}
    }

    OmQuery myq2_2 = OmQuery(OmQuery::OP_AND, v3.begin(), v3.end());
    if (myq2_2.get_length() != 3) {
	success = false;
	if (verbose) {
	    cout << "Query myq2_2 length is "
		    << myq2_2.get_length()
		    << ", expected 3.  Description: "
		    << myq2_2.get_description() << endl;
	}
    }

    OmQuery myq2 = OmQuery(OmQuery::OP_OR, myq2_1, myq2_2);
    if (myq2.get_length() != 7) {
	success = false;
	if (verbose) {
	    cout << "Query myq2 length is "
		    << myq2.get_length()
		    << ", expected 7.  Description: "
		    << myq2.get_description() << endl;
	}
    }

    myquery = OmQuery(OmQuery::OP_OR, myq1, myq2);
    if (myquery.get_length() != 10) {
	success = false;
	if (verbose) {
	    cout << "Query length is "
		 << myquery.get_length()
		 << ", expected 9"
		 << endl;
	    cout << "Query is: "
		 << myquery.get_description()
		 << endl;
	}
    }

    return success;
}

// tests that the collapsing on termpos optimisation works
static bool test_poscollapse1()
{
    OmQuery myquery1(OmQuery::OP_OR,
		     OmQuery("thi", 1, 1),
		     OmQuery("thi", 1, 1));
    OmQuery myquery2("thi", 2, 1);

    if (verbose) {
	cout << myquery1.get_description() << endl;
	cout << myquery2.get_description() << endl;
    }

    OmMSet mymset1 = do_get_simple_query_mset(myquery1);
    OmMSet mymset2 = do_get_simple_query_mset(myquery2);

    TEST_EQUAL(mymset1, mymset2);

    return true;
}

// tests that the collapsing on termpos optimisation gives correct query length
static bool test_poscollapse2()
{
    OmQuery q(OmQuery::OP_OR, OmQuery("thi", 1, 1), OmQuery("thi", 1, 1));
    TEST_EQUAL(q.get_length(), 2);
    return true;
}

// tests that collapsing of queries includes subqueries
static bool test_subqcollapse1()
{
    bool success = true;

    OmQuery queries1[3] = {
	OmQuery("wibble"),
	OmQuery("wobble"),
	OmQuery(OmQuery::OP_OR, std::string("jelly"), std::string("belly"))
    };

    OmQuery queries2[3] = {
	OmQuery(OmQuery::OP_AND, std::string("jelly"), std::string("belly")),
	OmQuery("wibble"),
	OmQuery("wobble")
    };

    vector<OmQuery> vec1(queries1, queries1+3);
    OmQuery myquery1(OmQuery::OP_OR, vec1.begin(), vec1.end());
    string desc1 = myquery1.get_description();

    vector<OmQuery> vec2(queries2, queries2+3);
    OmQuery myquery2(OmQuery::OP_AND, vec2.begin(), vec2.end());
    string desc2 = myquery2.get_description();

    if(desc1 != "OmQuery((wibble OR wobble OR jelly OR belly))") {
	success = false;
	if(verbose) {
	    cout << "Failed to correctly collapse query: got `" <<
		    desc1 << "'" << endl;
	}
    }

    if(desc2 != "OmQuery((jelly AND belly AND wibble AND wobble))") {
	success = false;
	if(verbose) {
	    cout << "Failed to correctly collapse query: got `" <<
		    desc2 << "'" << endl;
	}
    }

    return success;
}

// test that the batch query functionality works
static bool test_batchquery1()
{
    OmBatchEnquire::query_desc mydescs[3] = {
	{ OmQuery("thi"), 0, 10, 0, 0, 0},
	{ OmQuery(), 0, 10, 0, 0, 0},
	{ OmQuery("word"), 0, 10, 0, 0, 0}};

    OmBatchEnquire benq(get_simple_database());

    OmBatchEnquire::query_batch myqueries(mydescs, mydescs+3);

    benq.set_queries(myqueries);

    OmBatchEnquire::mset_batch myresults = benq.get_msets();

    TEST_EQUAL(myresults.size(), 3);
    TEST_EQUAL(myresults[0].value(), do_get_simple_query_mset(OmQuery("thi")));
    TEST(!myresults[1].is_valid());
    TEST_EXCEPTION(OmInvalidResultError, OmMSet unused = myresults[1].value());
    TEST_EQUAL(myresults[2].value(), do_get_simple_query_mset(OmQuery("word")));

    return true;
}

// test that running a query twice returns the same results
static bool test_repeatquery1()
{
    OmEnquire enquire(get_simple_database());
    init_simple_enquire(enquire);

    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery("thi"),
		    OmQuery("word"));
    enquire.set_query(myquery);

    OmMSet mymset1 = enquire.get_mset(0, 10);
    OmMSet mymset2 = enquire.get_mset(0, 10);
    TEST_EQUAL(mymset1, mymset2);

    return true;
}

// test that searching for a term not in the database fails nicely
static bool test_absentterm1()
{
    OmEnquire enquire(get_simple_database());
    OmQuery query("frink");
    query.set_bool(true);
    init_simple_enquire(enquire, query);

    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    return true;
}

// as absentterm1, but setting query from a vector of terms
static bool test_absentterm2()
{
    OmEnquire enquire(get_simple_database());
    vector<om_termname> terms;
    terms.push_back("frink");

    OmQuery query(OmQuery::OP_OR, terms.begin(), terms.end());
    enquire.set_query(query);

    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    return true;
}

// test behaviour when creating a query from an empty vector
static bool test_emptyquerypart1()
{
    vector<om_termname> emptyterms;
    OmQuery query(OmQuery::OP_OR, emptyterms.begin(), emptyterms.end());

    return true;
}

static bool test_stemlangs()
{
    vector<string> langs;
    langs = OmStem::get_available_languages();

    TEST(langs.size() != 0);

    vector<string>::const_iterator i;
    for (i = langs.begin(); i != langs.end(); i++) {
	// try making a stemmer with the given language -
	// it should successfully create, and not throw an exception.
	OmStem stemmer(*i);
    }

    return true;
}

// test that a multidb with 2 dbs query returns correct docids
static bool test_multidb3()
{
    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmEnquire enquire(make_dbgrp(&mydb2, &mydb3));

    // make a query
    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery("inmemory"),
		    OmQuery("word"));
    myquery.set_bool(true);
    enquire.set_query(myquery);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 2, 3, 7);

    return true;
}

// test that a multidb with 3 dbs query returns correct docids
static bool test_multidb4()
{
    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmDatabase mydb4(get_database("apitest_termorder"));
    OmEnquire enquire(make_dbgrp(&mydb2, &mydb3, &mydb4));

    // make a query
    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery("inmemory"),
		    OmQuery("word"));
    myquery.set_bool(true);
    enquire.set_query(myquery);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 2, 3, 4, 10);

    return true;
}

// test that rsets do sensible things
static bool test_rset1()
{
    bool success = true;

    OmDatabase mydb(get_database("apitest_rset"));

    OmEnquire enquire(make_dbgrp(&mydb));

    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery("giraff"),
		    OmQuery("tiger"));

    enquire.set_query(myquery);

    OmMSet mymset1 = enquire.get_mset(0, 10);

    OmRSet myrset;
    myrset.add_document(1);

    OmMSet mymset2 = enquire.get_mset(0, 10, &myrset);

    // We should have the same documents turn up, but 1 and 3 should
    // have higher weights with the RSet.
    if (mymset1.items.size() != 3 ||
	mymset2.items.size() != 3) {
	if (verbose) {
	    cout << "MSets are of different size: " << endl;
	    cout << "mset1: " << mymset1 << endl;
	    cout << "mset2: " << mymset2 << endl;
	}
	success = false;
    }

    return success;
}

// test that rsets do more sensible things
static bool test_rset2()
{
    bool success = true;

    OmDatabase mydb(get_database("apitest_rset"));

    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");

    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery(stemmer.stem_word("cuddly")),
		    OmQuery(stemmer.stem_word("people")));

    enquire.set_query(myquery);

    OmMSet mymset1 = enquire.get_mset(0, 10);

    OmRSet myrset;
    myrset.add_document(2);

    OmMSet mymset2 = enquire.get_mset(0, 10, &myrset);

    mset_expect_order(mymset1, 1, 2);
    mset_expect_order(mymset2, 2, 1);

    return success;
}

// test that rsets behave correctly with multiDBs
static bool test_rsetmultidb1()
{
    OmDatabase mydb1(get_database("apitest_rset", "apitest_simpledata2"));
    OmDatabase mydb2(get_database("apitest_rset"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));

    OmEnquire enquire1(make_dbgrp(&mydb1));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    OmStem stemmer("english");
    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery(stemmer.stem_word("cuddly")),
		    OmQuery(stemmer.stem_word("multiple")));

    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    OmRSet myrset1;
    OmRSet myrset2;
    myrset1.add_document(4);
    myrset2.add_document(2);

    OmMSet mymset1a = enquire1.get_mset(0, 10);
    OmMSet mymset1b = enquire1.get_mset(0, 10, &myrset1);
    OmMSet mymset2a = enquire2.get_mset(0, 10);
    OmMSet mymset2b = enquire2.get_mset(0, 10, &myrset2);

    mset_expect_order(mymset1a, 1, 4);
    mset_expect_order(mymset1b, 4, 1);
    mset_expect_order(mymset2a, 1, 2);
    mset_expect_order(mymset2b, 2, 1);

    mset_range_is_same_weights(mymset1a, 0, mymset2a, 0, 2);
    mset_range_is_same_weights(mymset1b, 0, mymset2b, 0, 2);
    TEST_NOT_EQUAL(mymset1a, mymset1b);
    TEST_NOT_EQUAL(mymset2a, mymset2b);

    return true;
}

// test that rsets behave correctly with multiDBs
static bool test_rsetmultidb2()
{
    OmDatabase mydb1(get_database("apitest_rset", "apitest_simpledata2"));
    OmDatabase mydb2(get_database("apitest_rset"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));

    OmEnquire enquire1(make_dbgrp(&mydb1));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    OmStem stemmer("english");
    OmQuery myquery(stemmer.stem_word("is"));

    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    OmRSet myrset1;
    OmRSet myrset2;
    myrset1.add_document(4);
    myrset2.add_document(2);

    OmMSet mymset1a = enquire1.get_mset(0, 10);
    OmMSet mymset1b = enquire1.get_mset(0, 10, &myrset1);
    OmMSet mymset2a = enquire2.get_mset(0, 10);
    OmMSet mymset2b = enquire2.get_mset(0, 10, &myrset2);

    mset_expect_order(mymset1a, 4, 3);
    mset_expect_order(mymset1b, 4, 3);
    mset_expect_order(mymset2a, 2, 5);
    mset_expect_order(mymset2b, 2, 5);

    mset_range_is_same_weights(mymset1a, 0, mymset2a, 0, 2);
    mset_range_is_same_weights(mymset1b, 0, mymset2b, 0, 2);
    TEST_NOT_EQUAL(mymset1a, mymset1b);
    TEST_NOT_EQUAL(mymset2a, mymset2b);

    return true;
}

/// Simple test of the match_max_or_terms option.
static bool test_maxorterms1()
{
    OmDatabase mydb(get_database("apitest_simpledata"));
    OmEnquire enquire(make_dbgrp(&mydb));

    OmStem stemmer("english");

#if 0
    std::string stemmed_word = stemmer.stem_word("word");
    OmQuery myquery1(stemmed_word);
#endif

    OmQuery myquery2(OmQuery::OP_OR,
		     OmQuery(stemmer.stem_word("simple")),
		     OmQuery(stemmer.stem_word("word")));

#if 0
    enquire.set_query(myquery1);
    OmMSet mymset1 = enquire.get_mset(0, 10);
#endif

    enquire.set_query(myquery2);
    OmSettings moptions;
    moptions.set("match_max_or_terms", 1);
    OmMSet mymset2 = enquire.get_mset(0, 10, NULL, &moptions);

#if 0
    // query lengths differ so mset weights not the same (at present)
    TEST_EQUAL(mymset1, mymset2);
#endif
    mset_expect_order(mymset2, 4, 2);

    return true;
}

/// Test the match_max_or_terms option works if the OR contains
/// sub-expressions (regression test)
static bool test_maxorterms2()
{
    OmDatabase mydb(get_database("apitest_simpledata"));
    OmEnquire enquire(make_dbgrp(&mydb));

    OmStem stemmer("english");

    OmQuery myquery1(OmQuery::OP_AND,
		     OmQuery(stemmer.stem_word("word")),
		     OmQuery(stemmer.stem_word("search")));

    OmQuery myquery2(OmQuery::OP_OR,
		     OmQuery(stemmer.stem_word("this")),
		     OmQuery(OmQuery::OP_AND,
			     OmQuery(stemmer.stem_word("word")),
			     OmQuery(stemmer.stem_word("search"))));

    enquire.set_query(myquery1);
    OmMSet mymset1 = enquire.get_mset(0, 10);

    enquire.set_query(myquery2);
    OmSettings moptions;
    moptions.set("match_max_or_terms", 1);
    OmMSet mymset2 = enquire.get_mset(0, 10, NULL, &moptions);

    // query lengths differ so mset weights not the same (at present)
    // TEST_EQUAL(mymset1, mymset2);
    test_mset_order_equal(mymset1, mymset2);

    return true;
}

/// Test that max_or_terms doesn't affect query results if we have fewer
/// terms than the threshold
static bool test_maxorterms3()
{
    OmDatabase mydb1(get_database("apitest_simpledata"));
    OmEnquire enquire1(make_dbgrp(&mydb1));

    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmEnquire enquire2(make_dbgrp(&mydb2));

    // make a query
    OmStem stemmer("english");

    string term1 = stemmer.stem_word("word");
    string term2 = stemmer.stem_word("inmemory");
    string term3 = stemmer.stem_word("flibble");

    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery(term1),
		    OmQuery(OmQuery::OP_OR,
			    OmQuery(term2),
			    OmQuery(term3)));
    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    OmSettings mopts;
    mopts.set("match_max_or_terms", 3);
    
    // retrieve the results
    OmMSet mymset1 = enquire1.get_mset(0, 10);
    OmMSet mymset2 = enquire2.get_mset(0, 10, NULL, &mopts);

    TEST_EQUAL(mymset1.get_termfreq(term1),
	       mymset2.get_termfreq(term1));
    TEST_EQUAL(mymset1.get_termweight(term1),
	       mymset2.get_termweight(term1));
    TEST_EQUAL(mymset1.get_termfreq(term2),
	       mymset2.get_termfreq(term2));
    TEST_EQUAL(mymset1.get_termweight(term2),
	       mymset2.get_termweight(term2));
    TEST_EQUAL(mymset1.get_termfreq(term3),
	       mymset2.get_termfreq(term3));
    TEST_EQUAL(mymset1.get_termweight(term3),
	       mymset2.get_termweight(term3));
//    TEST_EQUAL(mymset1, mymset2);

    return true;
}

/// Test that the termfreq returned by termlists is correct.
static bool test_termlisttermfreq()
{
    OmDatabase mydb(get_database("apitest_simpledata"));
    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");
    OmRSet rset1;
    OmRSet rset2;
    rset1.add_document(5);
    rset2.add_document(6);

    OmESet eset1 = enquire.get_eset(1000, rset1);
    OmESet eset2 = enquire.get_eset(1000, rset2);

    // search for weight of term 'another'
    string theterm = stemmer.stem_word("another");

    vector<OmESetItem>::const_iterator i;
    om_weight wt1 = 0;
    om_weight wt2 = 0;
    for(i = eset1.items.begin(); i != eset1.items.end(); i++) {
	if(i->tname == theterm) {
	    wt1 = i->wt;
	    break;
	}
    }
    for(i = eset2.items.begin(); i != eset2.items.end(); i++) {
	if(i->tname == theterm) {
	    wt2 = i->wt;
	    break;
	}
    }

    TEST_NOT_EQUAL(wt1, 0);
    TEST_NOT_EQUAL(wt2, 0);
    TEST_EQUAL(wt1, wt2);

    return true;
}

// tests an expand accross multiple databases
static bool test_multiexpand1()
{
    OmDatabase mydb1(get_database("apitest_simpledata", "apitest_simpledata2"));
    OmEnquire enquire1(make_dbgrp(&mydb1));

    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    // make simple equivalent rsets, with a document from each database in each.
    OmRSet rset1;
    OmRSet rset2;
    rset1.add_document(1);
    rset1.add_document(7);
    rset2.add_document(1);
    rset2.add_document(2);

    // retrieve the top ten results from each method of accessing
    // multiple text files

    // This is the single database one.
    OmESet eset1 = enquire1.get_eset(1000, rset1);

    // This is the multi database with approximation
    OmESet eset2 = enquire2.get_eset(1000, rset2);

    OmSettings eopts;
    eopts.set("expand_use_exact_termfreq", true);
    // This is the multi database without approximation
    OmESet eset3 = enquire2.get_eset(1000, rset2, &eopts);

    TEST_EQUAL(eset1.items.size(), eset2.items.size());
    TEST_EQUAL(eset1.items.size(), eset3.items.size());

    vector<OmESetItem>::const_iterator i;
    vector<OmESetItem>::const_iterator j;
    vector<OmESetItem>::const_iterator k;
    bool all_iwts_equal_jwts = true;
    for(i = eset1.items.begin(), j = eset2.items.begin(),
	k = eset3.items.begin();
	i != eset1.items.end(), j != eset2.items.end(), k != eset3.items.end();
	i++, j++, k++) {
	if (i->wt != j->wt) all_iwts_equal_jwts = false;
	TEST_EQUAL(i->wt, k->wt);
	TEST_EQUAL(i->tname, k->tname);
    }
    TEST(!all_iwts_equal_jwts);
    return true;
}

/// Simple test of NEAR
static bool test_near1()
{
    OmDatabase mydb(get_database("apitest_phrase"));
    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");

    // make a query
    vector<OmQuery> subqs;
    OmQuery q;
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 3);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 3);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1, 3);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 5);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1, 3);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 6);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1, 2, 3);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 3);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 4);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 5);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10, 11);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 6);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10, 11, 12);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 7);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top twenty results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 8);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    // test really large window size
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 999999999);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);

    return true;
}

/// Test NEAR over operators
static bool test_near2()
{
    OmDatabase mydb(get_database("apitest_phrase"));
    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");

    // make a query
    vector<OmQuery> subqs;
    OmQuery q;
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    subqs.push_back(OmQuery(stemmer.stem_word("and")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1);

    subqs.clear();
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    subqs.push_back(OmQuery(stemmer.stem_word("operator")));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 2);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("operator")));
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    q = OmQuery(OmQuery::OP_NEAR, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 2);

    return true;
}

/// Simple test of PHRASE
static bool test_phrase1()
{
    OmDatabase mydb(get_database("apitest_phrase"));
    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");

    // make a query
    vector<OmQuery> subqs;
    OmQuery q;
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 3);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 5);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("phrase")));
    subqs.push_back(OmQuery(stemmer.stem_word("near")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 6);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 1, 2);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 3);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 4);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 5);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 6);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 4);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 7);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top twenty results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 8);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top 20 results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4);

    // test really large window size
    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("leave")));
    subqs.push_back(OmQuery(stemmer.stem_word("fridge")));
    subqs.push_back(OmQuery(stemmer.stem_word("on")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 999999999);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top 20 results
    mymset = enquire.get_mset(0, 20);
    mset_expect_order(mymset, 4);

    // regression test (was matching doc 15, should fail)
    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("first")));
    subqs.push_back(OmQuery(stemmer.stem_word("second")));
    subqs.push_back(OmQuery(stemmer.stem_word("third")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 9);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    // regression test (should match doc 15, make sure still does with fix)
    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("first")));
    subqs.push_back(OmQuery(stemmer.stem_word("second")));
    subqs.push_back(OmQuery(stemmer.stem_word("third")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 10);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 15);

    // regression test (phrase matching was getting order wrong when
    // build_and_tree reordered vector of PostLists)
    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("milk")));
    subqs.push_back(OmQuery(stemmer.stem_word("rare")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 16);

    // regression test (phrase matching was getting order wrong when
    // build_and_tree reordered vector of PostLists)
    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("rare")));
    subqs.push_back(OmQuery(stemmer.stem_word("milk")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 17);

    return true;
}

/// Test PHRASE over operators
static bool test_phrase2()
{
    OmDatabase mydb(get_database("apitest_phrase"));
    OmEnquire enquire(make_dbgrp(&mydb));
    OmStem stemmer("english");

    // make a query
    vector<OmQuery> subqs;
    OmQuery q;
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    subqs.push_back(OmQuery(stemmer.stem_word("and")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    OmMSet mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    subqs.clear();
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    subqs.push_back(OmQuery(stemmer.stem_word("operator")));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset, 2);

    subqs.clear();
    subqs.push_back(OmQuery(stemmer.stem_word("operator")));
    subqs.push_back(OmQuery(OmQuery::OP_AND,
			    OmQuery(stemmer.stem_word("phrase")),
			    OmQuery(stemmer.stem_word("near"))));
    q = OmQuery(OmQuery::OP_PHRASE, subqs.begin(), subqs.end(), 2);
    q.set_bool(true);
    enquire.set_query(q);

    // retrieve the top ten results
    mymset = enquire.get_mset(0, 10);
    mset_expect_order(mymset);

    return true;
}

/// Test the termfrequency and termweight info returned for query terms
static bool test_qterminfo1()
{
    OmDatabase mydb1(get_database("apitest_simpledata", "apitest_simpledata2"));
    OmEnquire enquire1(make_dbgrp(&mydb1));

    OmDatabase mydb2(get_database("apitest_simpledata"));
    OmDatabase mydb3(get_database("apitest_simpledata2"));
    OmEnquire enquire2(make_dbgrp(&mydb2, &mydb3));

    // make a query
    OmStem stemmer("english");

    string term1 = stemmer.stem_word("word");
    string term2 = stemmer.stem_word("inmemory");
    string term3 = stemmer.stem_word("flibble");

    OmQuery myquery(OmQuery::OP_OR,
		    OmQuery(term1),
		    OmQuery(OmQuery::OP_OR,
			    OmQuery(term2),
			    OmQuery(term3)));
    enquire1.set_query(myquery);
    enquire2.set_query(myquery);

    // retrieve the results
    OmMSet mymset1a = enquire1.get_mset(0, 0);
    OmMSet mymset2a = enquire2.get_mset(0, 0);

    TEST_EQUAL(mymset1a.get_termfreq(term1),
	       mymset2a.get_termfreq(term1));
    TEST_EQUAL(mymset1a.get_termweight(term1),
	       mymset2a.get_termweight(term1));
    TEST_EQUAL(mymset1a.get_termfreq(term2),
	       mymset2a.get_termfreq(term2));
    TEST_EQUAL(mymset1a.get_termweight(term2),
	       mymset2a.get_termweight(term2));
    TEST_EQUAL(mymset1a.get_termfreq(term3),
	       mymset2a.get_termfreq(term3));
    TEST_EQUAL(mymset1a.get_termweight(term3),
	       mymset2a.get_termweight(term3));

    TEST_EQUAL(mymset1a.get_termfreq(term1), 3);
    TEST_EQUAL(mymset1a.get_termfreq(term2), 1);
    TEST_EQUAL(mymset1a.get_termfreq(term3), 0);

    TEST_NOT_EQUAL(mymset1a.get_termweight(term1), 0);
    TEST_NOT_EQUAL(mymset1a.get_termweight(term2), 0);
    // non-existant terms still have weight
    TEST_NOT_EQUAL(mymset1a.get_termweight(term3), 0);

    TEST_EXCEPTION(OmInvalidArgumentError,
		   mymset1a.get_termfreq("sponge"));

    return true;
}

// tests that when specifiying that no items are to be returned, those
// statistics which should be the same are.
static bool test_msetzeroitems1()
{
    OmMSet mymset1 = do_get_simple_query_mset(OmQuery("thi"), 0);
    OmMSet mymset2 = do_get_simple_query_mset(OmQuery("thi"), 1);

    TEST_EQUAL(mymset1.max_possible, mymset2.max_possible);

    return true;
}

// test that the mbound of a simple query is as expected
static bool test_mbound1()
{
    OmMSet mymset = do_get_simple_query_mset(OmQuery("word"));
    TEST_EQUAL(mymset.mbound, 2);
    return true;
}

#define CHECK_BACKEND_UNKNOWN(BACKEND) do {\
    OmSettings p;\
    p.set("backend", (BACKEND));\
    try { OmDatabase db(p);\
	FAIL_TEST("Backend `" << (BACKEND) << "' shouldn't be known but is"); }\
    catch (const OmInvalidArgumentError &e) { }\
    } while (0)

#define CHECK_BACKEND_UNAVAILABLE(BACKEND) do {\
    OmSettings p;\
    p.set("backend", (BACKEND));\
    try { OmDatabase db(p);\
	FAIL_TEST("Backend `" << (BACKEND) << "' shouldn't be available but is"); }\
    catch (const OmFeatureUnavailableError &e) { }\
    } while (0)

// test that DatabaseBuilder throws correct error for a completely unknown
// database backend, or if an empty string is passed for the backend
static bool test_badbackend1()
{
    CHECK_BACKEND_UNKNOWN("shorterofbreathanotherdayclosertodeath");
    CHECK_BACKEND_UNKNOWN("");
    return true;
}

// test that DatabaseBuilder throws correct error for any unavailable
// database backends
static bool test_badbackend2()
{
#ifndef MUS_BUILD_BACKEND_INMEMORY
    CHECK_BACKEND_UNAVAILABLE("inmemory");
#endif
#ifndef MUS_BUILD_BACKEND_QUARTZ
    CHECK_BACKEND_UNAVAILABLE("quartz");
#endif
#ifndef MUS_BUILD_BACKEND_SLEEPYCAT
    CHECK_BACKEND_UNAVAILABLE("sleepycat");
#endif
#ifndef MUS_BUILD_BACKEND_REMOTE
    CHECK_BACKEND_UNAVAILABLE("remote");
#endif
#ifndef MUS_BUILD_BACKEND_MUSCAT36
    CHECK_BACKEND_UNAVAILABLE("da");
    CHECK_BACKEND_UNAVAILABLE("db");
#endif
    return true;
}

// test that indexing a term more than once at the same position increases
// the wdf
static bool test_adddoc1()
{
    OmWritableDatabase db = backendmanager.get_writable_database("");

    OmDocumentContents doc1, doc2, doc3;

    // doc1 should come top, but if term "foo" gets wdf of 1, doc2 will beat it
    // doc3 should beat both
    // Note: all docs have same length
    doc1.data = string("tom");
    doc1.add_posting("foo", 1);
    doc1.add_posting("foo", 1);
    doc1.add_posting("foo", 1);
    doc1.add_posting("bar", 3);
    doc1.add_posting("bar", 4);
    db.add_document(doc1);
    
    doc2.data = string("dick");
    doc2.add_posting("foo", 1);
    doc2.add_posting("foo", 2);
    doc2.add_posting("bar", 3);
    doc2.add_posting("bar", 3);
    doc2.add_posting("bar", 3);
    db.add_document(doc2);

    doc3.data = string("harry");
    doc3.add_posting("foo", 1);
    doc3.add_posting("foo", 1);
    doc3.add_posting("foo", 2);
    doc3.add_posting("foo", 2);
    doc3.add_posting("bar", 3);
    db.add_document(doc3);

    OmQuery query("foo");

    OmEnquire enq(make_dbgrp(&db));
    enq.set_query(query);

    OmMSet mset = enq.get_mset(0, 10);

    mset_expect_order(mset, 3, 1, 2);

    return true;    
}

// tests that database destructors end_session if it isn't done explicitly
static bool test_implicitendsession()
{
    try {
	OmWritableDatabase db = backendmanager.get_writable_database("");

	db.begin_session();

	OmDocumentContents doc;
	
	doc.data = string("top secret");
	doc.add_posting("cia", 1);
	doc.add_posting("nsa", 2);
	doc.add_posting("fbi", 3);
	db.add_document(doc);
    }
    catch (...) {
	// in a debug build, an assertion in OmWritableDatabase's destructor
	// will fail at this point if the backend doesn't implicitly call
	// end_session()
	return false;
    }
    return true;
}

// tests that wqf affects the document weights
static bool test_wqf1()
{
    // both queries have length 2; in q1 word has wqf=2, in q2 word has wqf=1
    OmQuery q1("word", 2);
    OmQuery q2("word");
    q2.set_length(2);
    OmMSet mset1 = do_get_simple_query_mset(q1);
    OmMSet mset2 = do_get_simple_query_mset(q2);
    // Check the weights
    return (mset1.items[0].wt > mset2.items[0].wt);
}

// tests that query length affects the document weights
static bool test_qlen1()
{
    OmQuery q1("word");
    OmQuery q2("word");
    q2.set_length(2);
    OmMSet mset1 = do_get_simple_query_mset(q1);
    OmMSet mset2 = do_get_simple_query_mset(q2);
    // Check the weights
    return (mset1.items[0].wt < mset2.items[0].wt);
}

static bool test_databaseassign()
{
    OmWritableDatabase wdb = backendmanager.get_writable_database("");
    OmDatabase db = backendmanager.get_database("");
    OmDatabase actually_wdb = wdb;
    OmWritableDatabase w1(wdb);
    w1 = wdb;
    OmDatabase d1(wdb);
    OmDatabase d2(actually_wdb);
    d2 = wdb;
    d2 = actually_wdb;
    try { wdb = wdb; } // assign to itself
    catch (const OmInvalidArgumentError &e) { }
    try { db = db; } // assign to itself
    catch (const OmInvalidArgumentError &e) { }
    return true;
}

// #######################################################################
// # End of test cases: now we list the tests to run.

/// The tests which don't use any of the backends
test_desc nodb_tests[] = {
    {"trivial",            test_trivial},
    // {"alwaysfail",       test_alwaysfail},
    {"getqterms1",	   test_getqterms1},
    {"boolsubq1",	   test_boolsubq1},
    {"querylen1",	   test_querylen1},
    {"querylen2",	   test_querylen2},
    {"querylen3",	   test_querylen3},
    {"subqcollapse1",	   test_subqcollapse1},
    {"emptyquerypart1",    test_emptyquerypart1},
    {"stemlangs",	   test_stemlangs},
    {"badbackend1",	   test_badbackend1},
    {"badbackend2",	   test_badbackend2},
    {0, 0}
};

/// The tests which use a backend
test_desc db_tests[] = {
    {"zerodocid", 	   test_zerodocid},
    {"nullquery1",	   test_nullquery1},
    {"simplequery1",       test_simplequery1},
    {"simplequery2",       test_simplequery2},
    {"simplequery3",       test_simplequery3},
    {"multidb1",           test_multidb1},
    {"multidb2",           test_multidb2},
    {"changequery1",	   test_changequery1},
    {"msetmaxitems1",      test_msetmaxitems1},
    {"expandmaxitems1",    test_expandmaxitems1},
    {"boolquery1",         test_boolquery1},
    {"msetfirst1",         test_msetfirst1},
    {"topercent1",	   test_topercent1},
    {"expandfunctor1",	   test_expandfunctor1},
    {"pctcutoff1",	   test_pctcutoff1},
    {"allowqterms1",       test_allowqterms1},
    {"maxattain1",         test_maxattain1},
    {"collapsekey1",	   test_collapsekey1},
    {"reversebool1",	   test_reversebool1},
    {"reversebool2",	   test_reversebool2},
    {"getmterms1",	   test_getmterms1},
    {"absentfile1",	   test_absentfile1},
    {"poscollapse1",	   test_poscollapse1},
    {"poscollapse2",	   test_poscollapse2},
    {"batchquery1",	   test_batchquery1},
    {"repeatquery1",	   test_repeatquery1},
    {"absentterm1",	   test_absentterm1},
    {"absentterm2",	   test_absentterm2},
    {"multidb3",           test_multidb3},
    {"multidb4",           test_multidb4},
    {"rset1",              test_rset1},
    {"rset2",              test_rset2},
    {"rsetmultidb1",       test_rsetmultidb1},
    {"rsetmultidb2",       test_rsetmultidb2},
    {"maxorterms1",        test_maxorterms1},
    {"maxorterms2",        test_maxorterms2},
    {"maxorterms3",        test_maxorterms3},
    {"termlisttermfreq",   test_termlisttermfreq},
    {"qterminfo1",	   test_qterminfo1},
    {"msetzeroitems1",     test_msetzeroitems1},
    {"mbound1",            test_mbound1},
    {"wqf1",		   test_wqf1},
    {"qlen1",		   test_qlen1},
    {0, 0}
};

/// The tests which need a backend which supports positional information
test_desc positionaldb_tests[] = {
    {"near1",		   test_near1},
    {"near2",		   test_near2},
    {"phrase1",		   test_phrase1},
    {"phrase2",		   test_phrase2},
};

/// The tests which use a writable backend
test_desc writabledb_tests[] = {
    {"adddoc1",		   test_adddoc1},
    {"implicitendsession", test_implicitendsession},
    {"databaseassign",	   test_databaseassign},
    {0, 0}
};

test_desc localdb_tests[] = {
    {"matchfunctor1",	   test_matchfunctor1},
    {"multiexpand1",       test_multiexpand1},
    {0, 0}
};

test_desc muscat36da_tests[] = {
    {"zerodocid", 	   test_zerodocid},
    {"nullquery1",	   test_nullquery1},
    {"simplequery1",       test_simplequery1},
// get wrong weight back - probably because no document length in calcs
//    {"simplequery2",       test_simplequery2},
    {"simplequery3",       test_simplequery3},
    {"multidb1",           test_multidb1},
    {"multidb2",           test_multidb2},
    {"changequery1",	   test_changequery1},
    {"msetmaxitems1",      test_msetmaxitems1},
    {"expandmaxitems1",    test_expandmaxitems1},
    {"boolquery1",         test_boolquery1},
    {"msetfirst1",         test_msetfirst1},
    {"topercent1",	   test_topercent1},
    {"expandfunctor1",	   test_expandfunctor1},
// lack of document lengths means several hits come out with same weight
//    {"pctcutoff1",	   test_pctcutoff1},
    {"allowqterms1",       test_allowqterms1},
    {"maxattain1",         test_maxattain1},
    {"collapsekey1",	   test_collapsekey1},
    {"reversebool1",	   test_reversebool1},
    {"reversebool2",	   test_reversebool2},
    {"getmterms1",	   test_getmterms1},
    {"absentfile1",	   test_absentfile1},
    {"poscollapse1",	   test_poscollapse1},
    {"poscollapse2",	   test_poscollapse2},
    {"batchquery1",	   test_batchquery1},
    {"repeatquery1",	   test_repeatquery1},
    {"absentterm1",	   test_absentterm1},
    {"absentterm2",	   test_absentterm2},
    {"multidb3",           test_multidb3},
    {"multidb4",           test_multidb4},
    {"rset1",              test_rset1},
    {"rset2",              test_rset2},
    {"rsetmultidb1",       test_rsetmultidb1},
// Mset comes out in wrong order - no document length?
//    {"rsetmultidb2",       test_rsetmultidb2},
//    {"maxorterms1",        test_maxorterms1},
    {"maxorterms2",        test_maxorterms2},
    {"maxorterms3",        test_maxorterms3},
    {"termlisttermfreq",   test_termlisttermfreq},
    {"qterminfo1",	   test_qterminfo1},
    {"msetzeroitems1",     test_msetzeroitems1},
    {"mbound1",            test_mbound1},
    {"wqf1",		   test_wqf1},
    {"qlen1",		   test_qlen1},
    {0, 0}
};

#define RUNTESTS(B, T) if (backend.empty() || backend == (B)) {\
    test_driver::result sum_temp;\
    backendmanager.set_dbtype((B));\
    cout << "Running " << #T << " tests with " << (B) << " backend..." << endl;\
    result = max(result, test_driver::main(argc, argv, T##_tests, &sum_temp));\
    summary.succeeded += sum_temp.succeeded;\
    summary.failed += sum_temp.failed; } else (void)0

int main(int argc, char *argv[])
{
    string srcdir = test_driver::get_srcdir(argv[0]);
    string backend;
    const char *p = getenv("OM_TEST_BACKEND");
    if (p) backend = p;

    int result = 0;
    test_driver::result summary = {0, 0};

    backendmanager.set_datadir(srcdir + "/testdata/");

    RUNTESTS("void", nodb);

#if 1 && defined(MUS_BUILD_BACKEND_INMEMORY)
    RUNTESTS("inmemory", db);
    RUNTESTS("inmemory", writabledb);
    RUNTESTS("inmemory", localdb);
    RUNTESTS("inmemory", positionaldb);
#endif

#if 1 && defined(MUS_BUILD_BACKEND_QUARTZ)
    RUNTESTS("quartz", db);
    RUNTESTS("quartz", writabledb);
    RUNTESTS("quartz", localdb);
    RUNTESTS("quartz", positionaldb);
#endif

#if 1 && defined(MUS_BUILD_BACKEND_SLEEPYCAT)
    RUNTESTS("sleepycat", db);
    RUNTESTS("sleepycat", writabledb);
    RUNTESTS("sleepycat", localdb);
    RUNTESTS("sleepycat", positionaldb);
#endif

#if 1 && defined(MUS_BUILD_BACKEND_REMOTE)
    RUNTESTS("remote", db);
//    RUNTESTS("remote", positionaldb);
#endif

#if 1 && defined(MUS_BUILD_BACKEND_MUSCAT36)
    // need makeDA tool to build da databases
    if (file_exists("../../makeda/makeDA")) {
	RUNTESTS("da", muscat36da);
    }
#endif

    cout << argv[0] << " total: " << summary.succeeded << " passed, "
	 << summary.failed << " failed." << endl;

    return result;
}
