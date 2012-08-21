-- MySQL dump 10.13  Distrib 5.1.63, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: drizzle_stats
-- ------------------------------------------------------
-- Server version	5.1.63-0ubuntu0.10.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `bench_config`
--

DROP TABLE IF EXISTS `bench_config`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `bench_config` (
  `config_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`config_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `bench_config`
--

LOCK TABLES `bench_config` WRITE;
/*!40000 ALTER TABLE `bench_config` DISABLE KEYS */;
/*!40000 ALTER TABLE `bench_config` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `bench_runs`
--

DROP TABLE IF EXISTS `bench_runs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `bench_runs` (
  `run_id` int(11) NOT NULL AUTO_INCREMENT,
  `config_id` int(11) NOT NULL,
  `server` varchar(20) NOT NULL,
  `version` varchar(60) DEFAULT NULL,
  `run_date` datetime NOT NULL,
  PRIMARY KEY (`run_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `bench_runs`
--

LOCK TABLES `bench_runs` WRITE;
/*!40000 ALTER TABLE `bench_runs` DISABLE KEYS */;
/*!40000 ALTER TABLE `bench_runs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sysbench_run_iterations`
--

DROP TABLE IF EXISTS `sysbench_run_iterations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sysbench_run_iterations` (
  `run_id` int(11) NOT NULL,
  `concurrency` int(11) NOT NULL,
  `iteration` int(11) NOT NULL,
  `tps` decimal(13,2) NOT NULL,
  `read_write_req_per_second` decimal(13,2) NOT NULL,
  `deadlocks_per_second` decimal(5,2) NOT NULL,
  `min_req_latency_ms` decimal(10,2) NOT NULL,
  `avg_req_latency_ms` decimal(10,2) NOT NULL,
  `max_req_latency_ms` decimal(10,2) NOT NULL,
  `95p_req_latency_ms` decimal(10,2) NOT NULL,
  PRIMARY KEY (`run_id`,`concurrency`,`iteration`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sysbench_run_iterations`
--

LOCK TABLES `sysbench_run_iterations` WRITE;
/*!40000 ALTER TABLE `sysbench_run_iterations` DISABLE KEYS */;
/*!40000 ALTER TABLE `sysbench_run_iterations` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2012-08-14 12:50:51
