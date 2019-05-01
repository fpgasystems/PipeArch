DROP TABLE IF EXISTS netflix_model;
SELECT madlib.lmf_igd_run( 'netflix_model',
                           'netflix',
                           'Mindex',
                           'Uindex',
                           'Value',
                           4499,
                           470758,
                           32,
                           0.01,
                           1,
                           10,
                           1e-3);