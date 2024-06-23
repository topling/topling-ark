
int BOOST_JOIN(bdb_compare_var_, Name)(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	const unsigned char *end1, *end2;
	Type x1 = terark::BOOST_JOIN(load_var_, Name)((const unsigned char*)dbt1->data, &end1);
	Type x2 = terark::BOOST_JOIN(load_var_, Name)((const unsigned char*)dbt2->data, &end2);
	if (x1 < x2) return -1;
	if (x2 < x1) return  1;
	return 0;
}

int BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _n)
	(DB *db, const DBT *dbt1, const DBT *dbt2, int n)
{
	const unsigned char *p1 = (const unsigned char*)dbt1->data, *q1 = p1 + dbt1->size;
	const unsigned char *p2 = (const unsigned char*)dbt2->data, *q2 = p2 + dbt2->size;
	for (int i = 0; i != n; ++i)
	{
		Type x1 = terark::BOOST_JOIN(load_var_, Name)(p1, &p1);
		Type x2 = terark::BOOST_JOIN(load_var_, Name)(p2, &p2);
		if (x1 < x2) return -1;
		if (x2 < x1) return  1;
		if (p1 >= q1 || p2 >= q2)
			return dbt1->size - dbt2->size;
	}
	assert(p1 - (const unsigned char*)dbt1->data == p2 - (const unsigned char*)dbt2->data);
	return 0;
}

size_t BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _n)
	(DB *db, const DBT *dbt1, const DBT *dbt2, int n)
{
	const unsigned char* p1 = (const unsigned char*)dbt1->data, *q1 = p1 + dbt1->size;
	const unsigned char* p2 = (const unsigned char*)dbt2->data, *q2 = p2 + dbt2->size;
	for (int i = 0; i != n; ++i)
	{
		Type x1 = terark::BOOST_JOIN(load_var_, Name)(p1, &p1);
		Type x2 = terark::BOOST_JOIN(load_var_, Name)(p2, &p2);
		if (x1 != x2) break;
		if (p1 >= q1 || p2 >= q2)
			return (dbt1->size < dbt2->size ? dbt1->size : dbt2->size);
	}
	assert(p1 - (const unsigned char*)dbt1->data == p2 - (const unsigned char*)dbt2->data);
	return (p1 - (const unsigned char*)dbt1->data) + 1;
}

int BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _2)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _n)(db, dbt1, dbt2, 2);
}

size_t BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _2)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _n)(db, dbt1, dbt2, 2);
}

int BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _3)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _n)(db, dbt1, dbt2, 3);
}

size_t BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _3)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _n)(db, dbt1, dbt2, 3);
}

int BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _4)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_compare_var_, Name), _n)(db, dbt1, dbt2, 4);
}

size_t BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _4)
	(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return BOOST_JOIN(BOOST_JOIN(bdb_prefix_var_, Name), _n)(db, dbt1, dbt2, 4);
}

#undef Type
#undef Name

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



