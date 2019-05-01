DROP TABLE IF EXISTS netflix;
CREATE TABLE netflix (
	Mindex INTEGER NOT NULL,
	Uindex INTEGER NOT NULL,
	Value INTEGER NOT NULL);
COPY netflix(Mindex, Uindex, Value) FROM '/mnt/scratch/kkara/Datasets/Netflix/csvfiles/combined_data_1.csv' DELIMITER ',' CSV HEADER;