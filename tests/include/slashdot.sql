--
-- Table structure for table `abusers`
--

DROP TABLE IF EXISTS `abusers`;
CREATE TABLE `abusers` (
  `abuser_id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `ipid` varchar(32) NOT NULL default '',
  `subnetid` varchar(32) NOT NULL default '',
  `pagename` varchar(20) NOT NULL default '',
  `ts` datetime NOT NULL,
  `reason` varchar(120) NOT NULL default '',
  `querystring` varchar(200) NOT NULL default '',
  PRIMARY KEY  (`abuser_id`),
  KEY `uid` (`uid`),
  KEY `ipid` (`ipid`),
  KEY `subnetid` (`subnetid`),
  KEY `reason` (`reason`),
  KEY `ts` (`ts`)
);

--
-- Table structure for table `accesslog_build_unique_uid`
--

DROP TABLE IF EXISTS `accesslog_build_unique_uid`;
CREATE TABLE `accesslog_build_unique_uid` (
  `uid` bigint NOT NULL default '0',
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `accesslog_temp_host_addr`
--

DROP TABLE IF EXISTS `accesslog_temp_host_addr`;
CREATE TABLE `accesslog_temp_host_addr` (
  `host_addr` varchar(32) NOT NULL default '',
  `anon` enum('no','yes') NOT NULL default 'yes',
  PRIMARY KEY  (`host_addr`,`anon`),
  UNIQUE KEY `host_addr` (`host_addr`)
);

--
-- Table structure for table `achievements`
--

DROP TABLE IF EXISTS `achievements`;
CREATE TABLE `achievements` (
  `aid` bigint NOT NULL auto_increment,
  `name` varchar(30) NOT NULL default '',
  `description` varchar(128) NOT NULL default '',
  `repeatable` enum('yes','no') NOT NULL default 'no',
  `increment` int NOT NULL default '0',
  PRIMARY KEY  (`aid`),
  UNIQUE KEY `achievement` (`name`)
);

--
-- Table structure for table `ajax_ops`
--

DROP TABLE IF EXISTS `ajax_ops`;
CREATE TABLE `ajax_ops` (
  `id` bigint NOT NULL auto_increment,
  `op` varchar(50) NOT NULL default '',
  `class` varchar(100) NOT NULL default '',
  `subroutine` varchar(100) NOT NULL default '',
  `reskey_name` varchar(64) NOT NULL default '',
  `reskey_type` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `op` (`op`)
);

--
-- Table structure for table `al2`
--

DROP TABLE IF EXISTS `al2`;
CREATE TABLE `al2` (
  `srcid` bigint NOT NULL default '0',
  `value` int NOT NULL default '0',
  `updatecount` int NOT NULL default '0',
  PRIMARY KEY  (`srcid`),
  KEY `value` (`value`)
);

--
-- Table structure for table `al2_log`
--

DROP TABLE IF EXISTS `al2_log`;
CREATE TABLE `al2_log` (
  `al2lid` int NOT NULL auto_increment,
  `srcid` bigint NOT NULL default '0',
  `ts` datetime NOT NULL,
  `adminuid` bigint NOT NULL default '0',
  `al2tid` int NOT NULL default '0',
  `val` enum('set','clear') default NULL,
  PRIMARY KEY  (`al2lid`),
  KEY `ts` (`ts`),
  KEY `srcid_ts` (`srcid`,`ts`),
  KEY `al2tid_val_srcid` (`al2tid`,`val`,`srcid`)
);

--
-- Table structure for table `al2_log_comments`
--

DROP TABLE IF EXISTS `al2_log_comments`;
CREATE TABLE `al2_log_comments` (
  `al2lid` int NOT NULL default '0',
  `comment` text NOT NULL,
  PRIMARY KEY  (`al2lid`)
);

--
-- Table structure for table `al2_types`
--

DROP TABLE IF EXISTS `al2_types`;
CREATE TABLE `al2_types` (
  `al2tid` int NOT NULL auto_increment,
  `bitpos` int default NULL,
  `name` varchar(30) NOT NULL default '',
  `title` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`al2tid`),
  UNIQUE KEY `name` (`name`),
  UNIQUE KEY `bitpos` (`bitpos`)
);

--
-- Table structure for table `anniversary`
--

DROP TABLE IF EXISTS `anniversary`;
CREATE TABLE `anniversary` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `discussion_id` bigint NOT NULL default '0',
  `state` varchar(2) NOT NULL default '',
  `country` varchar(2) NOT NULL default '',
  `ts` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `venue` varchar(255) NOT NULL default '',
  `datetime` varchar(255) NOT NULL default '',
  `address` varchar(255) NOT NULL default '',
  `city` varchar(255) NOT NULL default '',
  `email` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`id`),
  KEY `uid` (`uid`)
);

--
-- Table structure for table `authors_cache`
--

DROP TABLE IF EXISTS `authors_cache`;
CREATE TABLE `authors_cache` (
  `uid` bigint NOT NULL auto_increment,
  `nickname` varchar(20) NOT NULL default '',
  `fakeemail` varchar(75) NOT NULL default '',
  `homepage` varchar(100) NOT NULL default '',
  `storycount` bigint NOT NULL default '0',
  `bio` text NOT NULL,
  `author` int NOT NULL default '0',
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `auto_poll`
--

DROP TABLE IF EXISTS `auto_poll`;
CREATE TABLE `auto_poll` (
  `id` bigint NOT NULL auto_increment,
  `primaryskid` int default NULL,
  `qid` bigint default NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `backup_blocks`
--

DROP TABLE IF EXISTS `backup_blocks`;
CREATE TABLE `backup_blocks` (
  `bid` varchar(30) NOT NULL default '',
  `block` text,
  PRIMARY KEY  (`bid`)
);

--
-- Table structure for table `badge_ids`
--

DROP TABLE IF EXISTS `badge_ids`;
CREATE TABLE `badge_ids` (
  `badge_id` int NOT NULL auto_increment,
  `badge_text` varchar(32) NOT NULL default '',
  `badge_url` varchar(255) NOT NULL default '',
  `badge_icon` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`badge_id`)
);

--
-- Table structure for table `badpasswords`
--

DROP TABLE IF EXISTS `badpasswords`;
CREATE TABLE `badpasswords` (
  `uid` bigint NOT NULL default '0',
  `ip` varchar(15) NOT NULL default '',
  `subnet` varchar(15) NOT NULL default '',
  `password` varchar(20) NOT NULL default '',
  `ts` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `realemail` varchar(50) NOT NULL default '',
  KEY `uid` (`uid`),
  KEY `ip` (`ip`),
  KEY `subnet` (`subnet`)
);

--
-- Table structure for table `blobs`
--

DROP TABLE IF EXISTS `blobs`;
CREATE TABLE `blobs` (
  `id` varchar(32) NOT NULL default '',
  `content_type` varchar(80) NOT NULL default '',
  `filename` varchar(80) NOT NULL default '',
  `seclev` bigint NOT NULL default '0',
  `reference_count` bigint NOT NULL default '1',
  `data` longblob NOT NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `blocks`
--

DROP TABLE IF EXISTS `blocks`;
CREATE TABLE `blocks` (
  `bid` varchar(30) NOT NULL default '',
  `block` text,
  `seclev` bigint NOT NULL default '0',
  `type` enum('static','portald') NOT NULL default 'static',
  `description` text,
  `skin` varchar(30) NOT NULL default '',
  `ordernum` int default '0',
  `title` varchar(128) NOT NULL default '',
  `portal` int NOT NULL default '0',
  `url` varchar(128) default NULL,
  `rdf` varchar(255) default NULL,
  `retrieve` int NOT NULL default '0',
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `rss_template` varchar(30) default NULL,
  `items` int NOT NULL default '0',
  `autosubmit` enum('no','yes') NOT NULL default 'no',
  `rss_cookie` varchar(255) default NULL,
  `all_skins` int NOT NULL default '0',
  `shill` enum('yes','no') NOT NULL default 'no',
  `shill_uid` bigint NOT NULL default '0',
  `id` bigint NOT NULL auto_increment,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `bid` (`bid`),
  KEY `type` (`type`),
  KEY `skin` (`skin`)
);

--
-- Table structure for table `bookmark_feeds`
--

DROP TABLE IF EXISTS `bookmark_feeds`;
CREATE TABLE `bookmark_feeds` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL,
  `feed` varchar(255) default NULL,
  `feedname` varchar(32) default NULL,
  `tags` varchar(255) default NULL,
  `nofilter` int NOT NULL default '0',
  `attended` enum('no','yes') NOT NULL default 'no',
  `firehose` enum('no','yes') default 'yes',
  `microbin` enum('no','yes') default 'no',
  PRIMARY KEY  (`id`),
  KEY `uid` (`uid`)
);

--
-- Table structure for table `bookmarks`
--

DROP TABLE IF EXISTS `bookmarks`;
CREATE TABLE `bookmarks` (
  `bookmark_id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `url_id` int NOT NULL,
  `createdtime` datetime NOT NULL,
  `title` varchar(255) default NULL,
  `srcid_32` bigint NOT NULL default '0',
  `srcid_24` bigint NOT NULL default '0',
  `srcname` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`bookmark_id`),
  UNIQUE KEY `url_id_uid` (`url_id`,`uid`),
  KEY `srcid_32` (`srcid_32`),
  KEY `srcid_24` (`srcid_24`)
);

--
-- Table structure for table `bpn_sources`
--

DROP TABLE IF EXISTS `bpn_sources`;
CREATE TABLE `bpn_sources` (
  `name` varchar(30) NOT NULL default '',
  `active` enum('no','yes') NOT NULL default 'yes',
  `source` varchar(255) NOT NULL default '',
  `regex` varchar(255) NOT NULL default '',
  `al2name` varchar(30) NOT NULL default 'nopostanon',
  PRIMARY KEY  (`name`)
);

--
-- Table structure for table `classes`
--

DROP TABLE IF EXISTS `classes`;
CREATE TABLE `classes` (
  `id` bigint NOT NULL auto_increment,
  `class` varchar(255) NOT NULL default '',
  `db_type` enum('writer','reader','log','search','log_slave') NOT NULL default 'writer',
  `fallback` enum('writer','reader','log','search','log_slave') default NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `class_key` (`class`)
);

--
-- Table structure for table `clout_types`
--

DROP TABLE IF EXISTS `clout_types`;
CREATE TABLE `clout_types` (
  `clid` int NOT NULL auto_increment,
  `name` varchar(16) NOT NULL,
  `class` varchar(255) NOT NULL,
  PRIMARY KEY  (`clid`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `code_param`
--

DROP TABLE IF EXISTS `code_param`;
CREATE TABLE `code_param` (
  `param_id` int NOT NULL auto_increment,
  `type` varchar(24) NOT NULL default '',
  `code` int NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `code_key` (`type`,`code`)
);

--
-- Table structure for table `comment_log`
--

DROP TABLE IF EXISTS `comment_log`;
CREATE TABLE `comment_log` (
  `id` int NOT NULL auto_increment,
  `cid` int NOT NULL,
  `logtext` varchar(255) NOT NULL default '',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `ts` (`ts`),
  KEY `cid` (`cid`)
);

--
-- Table structure for table `comment_promote_log`
--

DROP TABLE IF EXISTS `comment_promote_log`;
CREATE TABLE `comment_promote_log` (
  `id` int NOT NULL auto_increment,
  `cid` int NOT NULL default '0',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `cid` (`cid`)
);

--
-- Table structure for table `comment_text`
--

DROP TABLE IF EXISTS `comment_text`;
CREATE TABLE `comment_text` (
  `cid` int NOT NULL default '0',
  `comment` text NOT NULL,
  PRIMARY KEY  (`cid`)
);

--
-- Table structure for table `commentmodes`
--

DROP TABLE IF EXISTS `commentmodes`;
CREATE TABLE `commentmodes` (
  `mode` varchar(16) NOT NULL default '',
  `name` varchar(32) default NULL,
  `description` varchar(64) default NULL,
  PRIMARY KEY  (`mode`)
);

--
-- Table structure for table `comments`
--

DROP TABLE IF EXISTS `comments`;
CREATE TABLE `comments` (
  `sid` bigint NOT NULL default '0',
  `cid` int NOT NULL auto_increment,
  `pid` int NOT NULL default '0',
  `date` datetime NOT NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `ipid` varchar(32) NOT NULL default '',
  `subnetid` varchar(32) NOT NULL default '',
  `subject` varchar(50) NOT NULL default '',
  `subject_orig` enum('no','yes') NOT NULL default 'yes',
  `uid` bigint NOT NULL default '0',
  `points` int NOT NULL default '0',
  `pointsorig` int NOT NULL default '0',
  `pointsmax` int NOT NULL default '0',
  `f1` float default NULL,
  `f2` float default NULL,
  `f3` float default NULL,
  `f4` float default NULL,
  `f5` float default NULL,
  `f6` float default NULL,
  `lastmod` bigint NOT NULL default '0',
  `reason` int NOT NULL default '0',
  `signature` varchar(32) NOT NULL default '',
  `karma_bonus` enum('yes','no') NOT NULL default 'no',
  `subscriber_bonus` enum('no','yes') NOT NULL default 'no',
  `len` int NOT NULL default '0',
  `karma` int NOT NULL default '0',
  `karma_abs` int NOT NULL default '0',
  `tweak_orig` int NOT NULL default '0',
  `tweak` int NOT NULL default '0',
  `badge_id` int NOT NULL default '0',
  PRIMARY KEY  (`cid`),
  KEY `display` (`sid`,`points`,`uid`),
  KEY `byname` (`uid`,`points`),
  KEY `ipid` (`ipid`),
  KEY `uid` (`uid`),
  KEY `subnetid` (`subnetid`),
  KEY `theusual` (`sid`,`uid`,`points`,`cid`),
  KEY `countreplies` (`pid`,`sid`),
  KEY `uid_date` (`uid`,`date`),
  KEY `date_sid` (`date`,`sid`)
);

--
-- Table structure for table `content_filters`
--

DROP TABLE IF EXISTS `content_filters`;
CREATE TABLE `content_filters` (
  `filter_id` int NOT NULL auto_increment,
  `form` varchar(20) NOT NULL default '',
  `regex` varchar(100) NOT NULL default '',
  `modifier` varchar(5) NOT NULL default '',
  `field` varchar(20) NOT NULL default '',
  `ratio` float(6,4) NOT NULL default '0.0000',
  `minimum_match` bigint NOT NULL default '0',
  `minimum_length` bigint NOT NULL default '0',
  `err_message` varchar(150) default '',
  PRIMARY KEY  (`filter_id`),
  KEY `form` (`form`),
  KEY `regex` (`regex`),
  KEY `field_key` (`field`)
);

--
-- Table structure for table `css`
--

DROP TABLE IF EXISTS `css`;
CREATE TABLE `css` (
  `csid` int NOT NULL auto_increment,
  `rel` varchar(32) default 'stylesheet',
  `type` varchar(32) default 'text/css',
  `media` varchar(64) default NULL,
  `file` varchar(64) default NULL,
  `title` varchar(32) default NULL,
  `skin` varchar(32) default '',
  `page` varchar(32) default '',
  `admin` enum('no','yes') default 'no',
  `theme` varchar(32) default '',
  `ctid` int NOT NULL default '0',
  `ordernum` int default '0',
  `ie_cond` varchar(16) default '',
  `lowbandwidth` enum('no','yes') default 'no',
  `layout` varchar(16) default '',
  PRIMARY KEY  (`csid`),
  KEY `ctid` (`ctid`),
  KEY `page_skin` (`page`,`skin`),
  KEY `skin_page` (`skin`,`page`),
  KEY `layout` (`layout`)
);

--
-- Table structure for table `css_type`
--

DROP TABLE IF EXISTS `css_type`;
CREATE TABLE `css_type` (
  `ctid` int NOT NULL auto_increment,
  `name` varchar(32) NOT NULL default '',
  `ordernum` int NOT NULL default '0',
  PRIMARY KEY  (`ctid`)
);

--
-- Table structure for table `dateformats`
--

DROP TABLE IF EXISTS `dateformats`;
CREATE TABLE `dateformats` (
  `id` int NOT NULL default '0',
  `format` varchar(64) default NULL,
  `description` varchar(64) default NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `daypass_available`
--

DROP TABLE IF EXISTS `daypass_available`;
CREATE TABLE `daypass_available` (
  `daid` int NOT NULL auto_increment,
  `adnum` int NOT NULL default '0',
  `minduration` int NOT NULL default '0',
  `starttime` datetime NOT NULL,
  `endtime` datetime NOT NULL,
  `aclreq` varchar(32) default NULL,
  PRIMARY KEY  (`daid`)
);

--
-- Table structure for table `daypass_confcodes`
--

DROP TABLE IF EXISTS `daypass_confcodes`;
CREATE TABLE `daypass_confcodes` (
  `confcode` varchar(20) NOT NULL default '',
  `gooduntil` datetime NOT NULL,
  PRIMARY KEY  (`confcode`)
);

--
-- Table structure for table `daypass_keys`
--

DROP TABLE IF EXISTS `daypass_keys`;
CREATE TABLE `daypass_keys` (
  `dpkid` int NOT NULL auto_increment,
  `daypasskey` varchar(20) NOT NULL default '',
  `daid` int NOT NULL default '0',
  `key_given` datetime NOT NULL,
  `earliest_confirmable` datetime NOT NULL,
  `key_confirmed` datetime default NULL,
  PRIMARY KEY  (`dpkid`),
  UNIQUE KEY `daypasskey` (`daypasskey`),
  KEY `key_given` (`key_given`)
);

--
-- Table structure for table `daypass_needs`
--

DROP TABLE IF EXISTS `daypass_needs`;
CREATE TABLE `daypass_needs` (
  `type` enum('skin','site','article') NOT NULL default 'skin',
  `data` varchar(255) NOT NULL default '',
  `starttime` datetime NOT NULL,
  `endtime` datetime default NULL
);

--
-- Table structure for table `dbs`
--

DROP TABLE IF EXISTS `dbs`;
CREATE TABLE `dbs` (
  `id` bigint NOT NULL auto_increment,
  `virtual_user` varchar(100) NOT NULL default '',
  `isalive` enum('no','yes') NOT NULL default 'no',
  `type` enum('writer','reader','log','search','log_slave','querylog','sphinx') NOT NULL default 'reader',
  `weight` int NOT NULL default '1',
  `weight_adjust` float NOT NULL default '1',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `type_vu` (`type`,`virtual_user`)
);

--
-- Table structure for table `dbs_readerstatus`
--

DROP TABLE IF EXISTS `dbs_readerstatus`;
CREATE TABLE `dbs_readerstatus` (
  `ts` datetime NOT NULL,
  `dbid` bigint NOT NULL default '0',
  `was_alive` enum('no','yes') NOT NULL default 'yes',
  `was_reachable` enum('no','yes') default 'yes',
  `was_running` enum('no','yes') default 'yes',
  `slave_lag_secs` float default '0',
  `query_bog_secs` float default '0',
  `bog_rsqid` bigint default NULL,
  `had_weight` int default '1',
  `had_weight_adjust` float default '1',
  KEY `ts_dbid` (`ts`,`dbid`)
);

--
-- Table structure for table `dbs_readerstatus_queries`
--

DROP TABLE IF EXISTS `dbs_readerstatus_queries`;
CREATE TABLE `dbs_readerstatus_queries` (
  `rsqid` bigint NOT NULL auto_increment,
  `text` varchar(255) default NULL,
  PRIMARY KEY  (`rsqid`),
  KEY `text` (`text`)
);

--
-- Table structure for table `discussion_kinds`
--

DROP TABLE IF EXISTS `discussion_kinds`;
CREATE TABLE `discussion_kinds` (
  `dkid` int NOT NULL auto_increment,
  `name` varchar(30) NOT NULL default '',
  PRIMARY KEY  (`dkid`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `discussions`
--

DROP TABLE IF EXISTS `discussions`;
CREATE TABLE `discussions` (
  `id` bigint NOT NULL auto_increment,
  `dkid` int NOT NULL default '1',
  `stoid` bigint NOT NULL default '0',
  `sid` varchar(16) NOT NULL default '',
  `title` varchar(128) NOT NULL default '',
  `url` varchar(255) NOT NULL default '',
  `topic` int default NULL,
  `ts` datetime NOT NULL,
  `type` enum('open','recycle','archived') NOT NULL default 'open',
  `uid` bigint NOT NULL default '0',
  `commentcount` int NOT NULL default '0',
  `flags` enum('ok','delete','dirty') NOT NULL default 'ok',
  `primaryskid` int default NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `approved` int NOT NULL default '0',
  `commentstatus` enum('disabled','enabled','friends_only','friends_fof_only','no_foe','no_foe_eof','logged_in') NOT NULL default 'enabled',
  `archivable` enum('no','yes') NOT NULL default 'yes',
  PRIMARY KEY  (`id`),
  KEY `stoid` (`stoid`),
  KEY `sid` (`sid`),
  KEY `topic` (`topic`),
  KEY `primaryskid` (`primaryskid`,`ts`),
  KEY `type` (`type`,`uid`,`ts`)
);

--
-- Table structure for table `dst`
--

DROP TABLE IF EXISTS `dst`;
CREATE TABLE `dst` (
  `region` varchar(32) NOT NULL default '',
  `selectable` int NOT NULL default '0',
  `start_hour` int NOT NULL default '0',
  `start_wnum` int NOT NULL default '0',
  `start_wday` int NOT NULL default '0',
  `start_month` int NOT NULL default '0',
  `end_hour` int NOT NULL default '0',
  `end_wnum` int NOT NULL default '0',
  `end_wday` int NOT NULL default '0',
  `end_month` int NOT NULL default '0',
  PRIMARY KEY  (`region`)
);

--
-- Table structure for table `dynamic_blocks`
--

DROP TABLE IF EXISTS `dynamic_blocks`;
CREATE TABLE `dynamic_blocks` (
  `type_id` int NOT NULL default '0',
  `type` enum('portal','admin','user') NOT NULL default 'user',
  `private` enum('yes','no') NOT NULL default 'no',
  PRIMARY KEY  (`type_id`)
);

--
-- Table structure for table `dynamic_user_blocks`
--

DROP TABLE IF EXISTS `dynamic_user_blocks`;
CREATE TABLE `dynamic_user_blocks` (
  `bid` bigint NOT NULL auto_increment,
  `portal_id` bigint NOT NULL default '0',
  `type_id` int NOT NULL default '0',
  `uid` bigint NOT NULL default '0',
  `title` varchar(64) NOT NULL default '',
  `url` varchar(128) NOT NULL default '',
  `name` varchar(30) NOT NULL default '',
  `description` varchar(64) NOT NULL default '',
  `block` text,
  `seclev` bigint NOT NULL default '0',
  `created` datetime NOT NULL,
  `last_update` datetime NOT NULL,
  PRIMARY KEY  (`bid`),
  UNIQUE KEY `name` (`name`),
  UNIQUE KEY `idx_uid_name` (`uid`,`name`),
  KEY `idx_typeid` (`type_id`),
  KEY `idx_portalid` (`portal_id`)
);

--
-- Table structure for table `farm_globjid`
--

DROP TABLE IF EXISTS `farm_globjid`;
CREATE TABLE `farm_globjid` (
  `globjid` int NOT NULL,
  `x` int default NULL,
  `y` int default NULL,
  PRIMARY KEY  (`globjid`)
);

--
-- Table structure for table `farm_ipid`
--

DROP TABLE IF EXISTS `farm_ipid`;
CREATE TABLE `farm_ipid` (
  `ipid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`ipid`)
);

--
-- Table structure for table `farm_uid`
--

DROP TABLE IF EXISTS `farm_uid`;
CREATE TABLE `farm_uid` (
  `uid` int NOT NULL,
  `x` int default NULL,
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `file_queue`
--

DROP TABLE IF EXISTS `file_queue`;
CREATE TABLE `file_queue` (
  `fqid` int NOT NULL auto_increment,
  `stoid` bigint default NULL,
  `fhid` bigint default NULL,
  `file` varchar(255) default NULL,
  `action` enum('upload','thumbnails','sprite') default NULL,
  `blobid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`fqid`)
);

--
-- Table structure for table `firehose`
--

DROP TABLE IF EXISTS `firehose`;
CREATE TABLE `firehose` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `globjid` int NOT NULL default '0',
  `discussion` bigint NOT NULL default '0',
  `type` enum('submission','journal','bookmark','feed','story','vendor','misc','comment','discussion','project','tagname') default 'submission',
  `createtime` datetime NOT NULL,
  `popularity` float NOT NULL default '0',
  `editorpop` float NOT NULL default '0',
  `neediness` float NOT NULL default '0',
  `activity` float NOT NULL default '0',
  `accepted` enum('no','yes') default 'no',
  `rejected` enum('no','yes') default 'no',
  `public` enum('no','yes') default 'no',
  `attention_needed` enum('no','yes') default 'no',
  `is_spam` enum('no','yes') default 'no',
  `bayes_spam` enum('no','yes') default 'no',
  `collateral_spam` enum('no','yes') default 'no',
  `primaryskid` int default '0',
  `tid` int default NULL,
  `srcid` int NOT NULL default '0',
  `url_id` int NOT NULL default '0',
  `toptags` varchar(255) default '',
  `email` varchar(255) NOT NULL default '',
  `emaildomain` varchar(255) NOT NULL default '',
  `name` varchar(50) NOT NULL,
  `dept` varchar(100) NOT NULL default '',
  `ipid` varchar(32) NOT NULL default '',
  `subnetid` varchar(32) NOT NULL default '',
  `category` varchar(30) NOT NULL default '',
  `nexuslist` varchar(32) NOT NULL default '',
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `signoffs` varchar(255) NOT NULL default '',
  `stoid` bigint default '0',
  `body_length` bigint NOT NULL default '0',
  `word_count` bigint NOT NULL default '0',
  `srcname` varchar(32) NOT NULL default '',
  `mediatype` enum('none','text','video','image','audio') NOT NULL default 'none',
  `thumb` bigint default NULL,
  `offmainpage` enum('no','yes') NOT NULL default 'no',
  `sprite` varchar(128) NOT NULL default '',
  `sprite_info` text NOT NULL,
  `preview` enum('no','yes') default 'no',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `globjid` (`globjid`),
  KEY `createtime` (`createtime`),
  KEY `popularity` (`popularity`),
  KEY `editorpop` (`editorpop`),
  KEY `url_id` (`url_id`),
  KEY `neediness` (`neediness`),
  KEY `uid` (`uid`),
  KEY `last_update` (`last_update`),
  KEY `type_srcid` (`type`,`srcid`)
);

--
-- Table structure for table `firehose_history`
--

DROP TABLE IF EXISTS `firehose_history`;
CREATE TABLE `firehose_history` (
  `globjid` int NOT NULL default '0',
  `secsin` int NOT NULL default '0',
  `userpop` float NOT NULL default '0',
  `editorpop` float NOT NULL default '0',
  UNIQUE KEY `globjid_secsin` (`globjid`,`secsin`)
);

--
-- Table structure for table `firehose_ogaspt`
--

DROP TABLE IF EXISTS `firehose_ogaspt`;
CREATE TABLE `firehose_ogaspt` (
  `globjid` int NOT NULL default '0',
  `pubtime` datetime NOT NULL,
  PRIMARY KEY  (`globjid`)
);

--
-- Table structure for table `firehose_section`
--

DROP TABLE IF EXISTS `firehose_section`;
CREATE TABLE `firehose_section` (
  `fsid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `section_name` varchar(32) NOT NULL default 'unnamed',
  `section_filter` varchar(255) NOT NULL default '',
  `skid` int NOT NULL default '0',
  `display` enum('yes','no') default 'yes',
  `view_id` bigint NOT NULL default '0',
  `ordernum` int default '0',
  `section_color` varchar(16) NOT NULL default '',
  PRIMARY KEY  (`fsid`)
);

--
-- Table structure for table `firehose_section_settings`
--

DROP TABLE IF EXISTS `firehose_section_settings`;
CREATE TABLE `firehose_section_settings` (
  `id` bigint NOT NULL auto_increment,
  `fsid` bigint NOT NULL,
  `uid` bigint NOT NULL default '0',
  `section_name` varchar(32) NOT NULL default 'unnamed',
  `section_filter` varchar(255) NOT NULL default '',
  `display` enum('yes','no') default 'yes',
  `view_id` bigint NOT NULL default '0',
  `section_color` varchar(16) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `uid_fsid` (`uid`,`fsid`)
);

--
-- Table structure for table `firehose_setting_log`
--

DROP TABLE IF EXISTS `firehose_setting_log`;
CREATE TABLE `firehose_setting_log` (
  `id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `value` varchar(64) NOT NULL default '',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  `ipid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `firehose_skin_volume`
--

DROP TABLE IF EXISTS `firehose_skin_volume`;
CREATE TABLE `firehose_skin_volume` (
  `skid` int NOT NULL,
  `story_vol` bigint NOT NULL default '0',
  `other_vol` bigint NOT NULL default '0',
  PRIMARY KEY  (`skid`)
);

--
-- Table structure for table `firehose_tab`
--

DROP TABLE IF EXISTS `firehose_tab`;
CREATE TABLE `firehose_tab` (
  `tabid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `tabname` varchar(16) NOT NULL default 'unnamed',
  `filter` varchar(255) NOT NULL default '',
  `orderby` enum('popularity','createtime','editorpop','activity') default 'createtime',
  `orderdir` enum('ASC','DESC') default 'DESC',
  `color` varchar(16) NOT NULL default '',
  `mode` enum('full','fulltitle') default 'fulltitle',
  PRIMARY KEY  (`tabid`),
  UNIQUE KEY `uid_tabname` (`uid`,`tabname`)
);

--
-- Table structure for table `firehose_text`
--

DROP TABLE IF EXISTS `firehose_text`;
CREATE TABLE `firehose_text` (
  `id` bigint NOT NULL,
  `title` varchar(80) default NULL,
  `introtext` text,
  `bodytext` text,
  `media` text,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `firehose_tfh`
--

DROP TABLE IF EXISTS `firehose_tfh`;
CREATE TABLE `firehose_tfh` (
  `uid` bigint NOT NULL,
  `globjid` int NOT NULL,
  UNIQUE KEY `uid_globjid` (`uid`,`globjid`),
  KEY `globjid` (`globjid`)
);

--
-- Table structure for table `firehose_tfhp`
--

DROP TABLE IF EXISTS `firehose_tfhp`;
CREATE TABLE `firehose_tfhp` (
  `uid` bigint NOT NULL,
  `globjid` int NOT NULL,
  UNIQUE KEY `uid_globjid` (`uid`,`globjid`),
  KEY `globjid` (`globjid`)
);

--
-- Table structure for table `firehose_topics_rendered`
--

DROP TABLE IF EXISTS `firehose_topics_rendered`;
CREATE TABLE `firehose_topics_rendered` (
  `id` bigint NOT NULL,
  `tid` int NOT NULL,
  UNIQUE KEY `id_tid` (`id`,`tid`),
  KEY `tid_id` (`tid`,`id`)
);

--
-- Table structure for table `firehose_update_log`
--

DROP TABLE IF EXISTS `firehose_update_log`;
CREATE TABLE `firehose_update_log` (
  `id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `new_count` int NOT NULL default '0',
  `update_count` int NOT NULL default '0',
  `total_num` int NOT NULL default '0',
  `more_num` int NOT NULL default '0',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  `duration` float NOT NULL default '0',
  `bytes` bigint NOT NULL default '0',
  `view` varchar(24) NOT NULL default '',
  `ipid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `firehose_update_log_temp`
--

DROP TABLE IF EXISTS `firehose_update_log_temp`;
CREATE TABLE `firehose_update_log_temp` (
  `id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `new_count` int NOT NULL default '0',
  `update_count` int NOT NULL default '0',
  `total_num` int NOT NULL default '0',
  `more_num` int NOT NULL default '0',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  `duration` float NOT NULL default '0',
  `bytes` bigint NOT NULL default '0',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `firehose_view`
--

DROP TABLE IF EXISTS `firehose_view`;
CREATE TABLE `firehose_view` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `viewname` varchar(24) default 'unnamed',
  `viewtitle` varchar(24) NOT NULL default '',
  `useparentfilter` enum('no','yes') default 'yes',
  `tab_display` enum('no','yes') default 'no',
  `options_edit` enum('no','yes') default 'no',
  `admin_maxitems` int NOT NULL default '-1',
  `maxitems` int NOT NULL default '-1',
  `seclev` bigint NOT NULL default '0',
  `filter` varchar(255) NOT NULL default '',
  `orderby` enum('popularity','createtime','editorpop','activity','neediness','') default 'createtime',
  `orderdir` enum('ASC','DESC','') default 'DESC',
  `color` varchar(16) NOT NULL default '',
  `duration` enum('7','-1','') default '',
  `mode` enum('full','fulltitle','mixed','') default '',
  `pause` enum('1','0','') default '',
  `searchbutton` enum('no','yes') default 'yes',
  `datafilter` varchar(128) NOT NULL default '',
  `admin_unsigned` enum('no','yes') default 'no',
  `usermode` enum('no','yes') default 'yes',
  `use_exclusions` enum('no','yes') default 'yes',
  `editable` enum('no','yes') default 'yes',
  `shortcut` enum('yes','no') default 'no',
  `short_url` varchar(32) NOT NULL default '',
  `link_icon` enum('no','yes') default 'no',
  `placeholder` enum('no','yes') default 'no',
  `addable` enum('no','yes') default 'no',
  `removable` enum('no','yes') default 'no',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `firehose_view_settings`
--

DROP TABLE IF EXISTS `firehose_view_settings`;
CREATE TABLE `firehose_view_settings` (
  `uid` bigint NOT NULL default '0',
  `id` bigint NOT NULL,
  `color` varchar(16) NOT NULL default '',
  `orderby` enum('popularity','createtime','editorpop','activity','neediness','') default 'createtime',
  `orderdir` enum('ASC','DESC','') default 'DESC',
  `mode` enum('full','fulltitle','mixed','') default '',
  `datafilter` varchar(128) NOT NULL default '',
  `admin_unsigned` enum('no','yes') default 'no',
  `usermode` enum('no','yes') default 'yes',
  PRIMARY KEY  (`uid`,`id`)
);

--
-- Table structure for table `formkeys`
--

DROP TABLE IF EXISTS `formkeys`;
CREATE TABLE `formkeys` (
  `formkey` varchar(20) NOT NULL default '',
  `formname` varchar(32) NOT NULL default '',
  `id` varchar(30) NOT NULL default '',
  `uid` bigint NOT NULL default '0',
  `ipid` varchar(32) NOT NULL default '',
  `value` int NOT NULL default '0',
  `ts` int NOT NULL default '0',
  `submit_ts` int NOT NULL default '0',
  `content_length` int NOT NULL default '0',
  `idcount` bigint NOT NULL default '0',
  `last_ts` int NOT NULL default '0',
  `subnetid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`formkey`),
  KEY `formname` (`formname`),
  KEY `ts` (`ts`),
  KEY `submit_ts` (`submit_ts`),
  KEY `idcount` (`idcount`),
  KEY `last_ts` (`last_ts`),
  KEY `uid` (`uid`),
  KEY `subnetid` (`subnetid`),
  KEY `ipid` (`ipid`)
);

--
-- Table structure for table `globj_adminnotes`
--

DROP TABLE IF EXISTS `globj_adminnotes`;
CREATE TABLE `globj_adminnotes` (
  `globjid` int NOT NULL auto_increment,
  `adminnote` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`globjid`)
);

--
-- Table structure for table `globj_types`
--

DROP TABLE IF EXISTS `globj_types`;
CREATE TABLE `globj_types` (
  `gtid` int NOT NULL auto_increment,
  `maintable` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`gtid`),
  UNIQUE KEY `maintable` (`maintable`)
);

--
-- Table structure for table `globj_urls`
--

DROP TABLE IF EXISTS `globj_urls`;
CREATE TABLE `globj_urls` (
  `id` int NOT NULL auto_increment,
  `globjid` int NOT NULL default '0',
  `url_id` int NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `globjid_url_id` (`globjid`,`url_id`)
);

--
-- Table structure for table `globjs`
--

DROP TABLE IF EXISTS `globjs`;
CREATE TABLE `globjs` (
  `globjid` int NOT NULL auto_increment,
  `gtid` int NOT NULL default '0',
  `target_id` int NOT NULL default '0',
  PRIMARY KEY  (`globjid`),
  UNIQUE KEY `target` (`gtid`,`target_id`)
);

--
-- Table structure for table `globjs_viewed`
--

DROP TABLE IF EXISTS `globjs_viewed`;
CREATE TABLE `globjs_viewed` (
  `gvid` int NOT NULL auto_increment,
  `globjid` int NOT NULL,
  `uid` bigint NOT NULL,
  `viewed_at` datetime NOT NULL,
  PRIMARY KEY  (`gvid`),
  UNIQUE KEY `globjid_uid` (`globjid`,`uid`)
);

--
-- Table structure for table `globjs_viewed_archived`
--

DROP TABLE IF EXISTS `globjs_viewed_archived`;
CREATE TABLE `globjs_viewed_archived` (
  `gvid` int NOT NULL,
  `globjid` int NOT NULL,
  `uid` bigint NOT NULL,
  `viewed_at` datetime NOT NULL,
  PRIMARY KEY  (`gvid`),
  UNIQUE KEY `globjid_uid` (`globjid`,`uid`)
);

--
-- Table structure for table `hooks`
--

DROP TABLE IF EXISTS `hooks`;
CREATE TABLE `hooks` (
  `id` bigint NOT NULL auto_increment,
  `param` varchar(50) NOT NULL default '',
  `class` varchar(100) NOT NULL default '',
  `subroutine` varchar(100) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `hook_param` (`param`,`class`,`subroutine`)
);

--
-- Table structure for table `humanconf`
--

DROP TABLE IF EXISTS `humanconf`;
CREATE TABLE `humanconf` (
  `hcid` int NOT NULL auto_increment,
  `hcpid` int NOT NULL,
  `formkey` varchar(20) NOT NULL default '',
  `tries_left` int NOT NULL default '3',
  PRIMARY KEY  (`hcid`),
  UNIQUE KEY `formkey` (`formkey`),
  KEY `hcpid` (`hcpid`)
);

--
-- Table structure for table `humanconf_pool`
--

DROP TABLE IF EXISTS `humanconf_pool`;
CREATE TABLE `humanconf_pool` (
  `hcpid` int NOT NULL auto_increment,
  `hcqid` int NOT NULL default '0',
  `answer` char NOT NULL default '',
  `lastused` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `created_at` datetime NOT NULL,
  `inuse` int NOT NULL default '0',
  `filename_img` varchar(63) NOT NULL,
  `filename_mp3` varchar(63) default NULL,
  `html` text NOT NULL,
  PRIMARY KEY  (`hcpid`),
  KEY `answer` (`answer`),
  KEY `lastused` (`lastused`)
);

--
-- Table structure for table `humanconf_questions`
--

DROP TABLE IF EXISTS `humanconf_questions`;
CREATE TABLE `humanconf_questions` (
  `hcqid` int NOT NULL auto_increment,
  `filedir` varchar(255) NOT NULL default '',
  `urlprefix` varchar(255) NOT NULL default '',
  `question` text NOT NULL,
  PRIMARY KEY  (`hcqid`)
);

--
-- Table structure for table `journal_themes`
--

DROP TABLE IF EXISTS `journal_themes`;
CREATE TABLE `journal_themes` (
  `id` int NOT NULL auto_increment,
  `name` varchar(30) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `journal_transfer`
--

DROP TABLE IF EXISTS `journal_transfer`;
CREATE TABLE `journal_transfer` (
  `id` bigint NOT NULL default '0',
  `subid` bigint NOT NULL default '0',
  `stoid` bigint NOT NULL default '0',
  `updated` int NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `stoid_updated` (`stoid`,`updated`)
);

--
-- Table structure for table `journals`
--

DROP TABLE IF EXISTS `journals`;
CREATE TABLE `journals` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `date` datetime NOT NULL,
  `description` varchar(80) NOT NULL default '',
  `posttype` int NOT NULL default '2',
  `discussion` bigint default NULL,
  `tid` int NOT NULL default '0',
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `promotetype` enum('publicize','publish','post') NOT NULL default 'publish',
  `srcid_32` bigint NOT NULL default '0',
  `srcid_24` bigint NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `uidarticle` (`uid`),
  KEY `IDandUID` (`id`,`uid`),
  KEY `uid_date_id` (`uid`,`date`,`id`),
  KEY `srcid_32` (`srcid_32`),
  KEY `srcid_24` (`srcid_24`)
);

--
-- Table structure for table `journals_text`
--

DROP TABLE IF EXISTS `journals_text`;
CREATE TABLE `journals_text` (
  `id` bigint NOT NULL default '0',
  `article` text NOT NULL,
  `introtext` text NOT NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `links`
--

DROP TABLE IF EXISTS `links`;
CREATE TABLE `links` (
  `id` varchar(32) NOT NULL default '',
  `url` text NOT NULL,
  `last_seen` datetime NOT NULL,
  `is_alive` enum('yes','no') NOT NULL default 'yes',
  `stats_type` varchar(24) default NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `links_for_stories`
--

DROP TABLE IF EXISTS `links_for_stories`;
CREATE TABLE `links_for_stories` (
  `stoid` bigint NOT NULL,
  `id` varchar(32) NOT NULL default '',
  `count` int NOT NULL default '0',
  PRIMARY KEY  (`id`,`stoid`),
  KEY `stoid` (`stoid`)
);

--
-- Table structure for table `memcached_stats`
--

DROP TABLE IF EXISTS `memcached_stats`;
CREATE TABLE `memcached_stats` (
  `id` int NOT NULL auto_increment,
  `ts` datetime NOT NULL,
  `secsold` int NOT NULL,
  `prefix` varchar(250) NOT NULL default '',
  `count` int NOT NULL,
  `bytes` int NOT NULL,
  `hits` int NOT NULL,
  `elapsed` float NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `ts_prefix` (`ts`,`prefix`),
  KEY `prefix_secsold` (`prefix`,`secsold`)
);

--
-- Table structure for table `menus`
--

DROP TABLE IF EXISTS `menus`;
CREATE TABLE `menus` (
  `id` bigint NOT NULL auto_increment,
  `menu` varchar(20) NOT NULL default '',
  `label` varchar(255) NOT NULL default '',
  `sel_label` varchar(32) NOT NULL default '',
  `value` text,
  `seclev` bigint NOT NULL default '0',
  `showanon` int NOT NULL default '0',
  `menuorder` bigint default NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `page_labels_un` (`menu`,`label`),
  KEY `page_labels` (`menu`,`label`)
);

--
-- Table structure for table `message_codes`
--

DROP TABLE IF EXISTS `message_codes`;
CREATE TABLE `message_codes` (
  `code` int NOT NULL default '0',
  `type` varchar(32) NOT NULL default '',
  `seclev` int NOT NULL default '1',
  `modes` varchar(32) NOT NULL default '',
  `subscribe` int NOT NULL default '0',
  `send` enum('now','defer','collective') NOT NULL default 'now',
  `acl` varchar(32) NOT NULL default '',
  `delivery_bvalue` int NOT NULL default '0',
  PRIMARY KEY  (`code`)
);

--
-- Table structure for table `message_deliverymodes`
--

DROP TABLE IF EXISTS `message_deliverymodes`;
CREATE TABLE `message_deliverymodes` (
  `code` int NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `bitvalue` bigint NOT NULL default '0',
  PRIMARY KEY  (`code`)
);

--
-- Table structure for table `message_drop`
--

DROP TABLE IF EXISTS `message_drop`;
CREATE TABLE `message_drop` (
  `id` int NOT NULL auto_increment,
  `user` bigint NOT NULL default '0',
  `fuser` bigint NOT NULL default '0',
  `code` int NOT NULL default '-1',
  `date` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `altto` varchar(50) NOT NULL default '',
  `message` blob NOT NULL,
  `send` enum('now','defer','collective') NOT NULL default 'now',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `message_log`
--

DROP TABLE IF EXISTS `message_log`;
CREATE TABLE `message_log` (
  `id` int NOT NULL default '0',
  `user` bigint NOT NULL default '0',
  `fuser` bigint NOT NULL default '0',
  `code` int NOT NULL default '-1',
  `mode` int NOT NULL default '0',
  `date` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP
);

--
-- Table structure for table `message_web`
--

DROP TABLE IF EXISTS `message_web`;
CREATE TABLE `message_web` (
  `id` int NOT NULL default '0',
  `user` bigint NOT NULL default '0',
  `fuser` bigint NOT NULL default '0',
  `code` int NOT NULL default '-1',
  `updated` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `readed` int NOT NULL default '0',
  `date` timestamp NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `fuser` (`fuser`),
  KEY `user` (`user`)
);

--
-- Table structure for table `message_web_text`
--

DROP TABLE IF EXISTS `message_web_text`;
CREATE TABLE `message_web_text` (
  `id` int NOT NULL default '0',
  `subject` blob NOT NULL,
  `message` blob NOT NULL,
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `metamodlog`
--

DROP TABLE IF EXISTS `metamodlog`;
CREATE TABLE `metamodlog` (
  `mmid` int NOT NULL default '0',
  `uid` bigint NOT NULL default '0',
  `val` int NOT NULL default '0',
  `ts` datetime default NULL,
  `id` int NOT NULL auto_increment,
  `active` int NOT NULL default '1',
  PRIMARY KEY  (`id`),
  KEY `mmid` (`mmid`),
  KEY `byuser` (`uid`)
);

--
-- Table structure for table `microbin`
--

DROP TABLE IF EXISTS `microbin`;
CREATE TABLE `microbin` (
  `id` bigint NOT NULL auto_increment,
  `username` varchar(32) NOT NULL default '',
  `src` varchar(64) NOT NULL default '',
  `tags` varchar(64) NOT NULL default '',
  `ts` datetime NOT NULL default '1970-01-01 00:00:00',
  `status` varchar(255) NOT NULL default '',
  `active` enum('yes','no') NOT NULL default 'yes',
  `introtext` text NOT NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `status` (`status`)
);

--
-- Table structure for table `misc_user_opts`
--

DROP TABLE IF EXISTS `misc_user_opts`;
CREATE TABLE `misc_user_opts` (
  `name` varchar(32) NOT NULL default '',
  `optorder` bigint default NULL,
  `seclev` bigint NOT NULL default '0',
  `default_val` text NOT NULL,
  `vals_regex` text,
  `short_desc` text,
  `long_desc` text,
  `opts_html` text,
  PRIMARY KEY  (`name`)
);

--
-- Table structure for table `moderatorlog`
--

DROP TABLE IF EXISTS `moderatorlog`;
CREATE TABLE `moderatorlog` (
  `id` int NOT NULL auto_increment,
  `ipid` varchar(32) NOT NULL default '',
  `subnetid` varchar(32) NOT NULL default '',
  `uid` bigint NOT NULL default '0',
  `val` int NOT NULL default '0',
  `sid` bigint NOT NULL default '0',
  `ts` datetime NOT NULL,
  `cid` int NOT NULL default '0',
  `reason` int default '0',
  `active` int NOT NULL default '1',
  `spent` int NOT NULL default '1',
  `m2count` bigint NOT NULL default '0',
  `m2needed` bigint default '0',
  `cuid` bigint default NULL,
  `m2status` int NOT NULL default '0',
  `points_orig` int default NULL,
  PRIMARY KEY  (`id`),
  KEY `sid` (`sid`,`cid`),
  KEY `sid_2` (`cid`,`uid`,`sid`),
  KEY `ipid` (`ipid`),
  KEY `subnetid` (`subnetid`),
  KEY `uid` (`uid`),
  KEY `cuid` (`cuid`),
  KEY `m2stat_act` (`m2status`,`active`),
  KEY `ts_uid_sid` (`ts`,`uid`,`sid`)
);

--
-- Table structure for table `modreasons`
--

DROP TABLE IF EXISTS `modreasons`;
CREATE TABLE `modreasons` (
  `id` int NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `m2able` int NOT NULL default '1',
  `listable` int NOT NULL default '1',
  `val` int NOT NULL default '0',
  `karma` int NOT NULL default '0',
  `fairfrac` float NOT NULL default '0.5',
  `unfairname` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`)
);

--
-- Table structure for table `open_proxies`
--

DROP TABLE IF EXISTS `open_proxies`;
CREATE TABLE `open_proxies` (
  `ip` varchar(15) NOT NULL default '',
  `port` int NOT NULL default '0',
  `dur` float default NULL,
  `ts` datetime NOT NULL,
  `xff` varchar(40) default NULL,
  `ipid` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`ip`),
  KEY `ts` (`ts`),
  KEY `xff` (`xff`),
  KEY `ipid` (`ipid`)
);

--
-- Table structure for table `people`
--

DROP TABLE IF EXISTS `people`;
CREATE TABLE `people` (
  `id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `person` bigint NOT NULL default '0',
  `type` enum('friend','foe') default NULL,
  `perceive` enum('fan','freak') default NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `degree_of_separation` (`uid`,`person`),
  KEY `person` (`person`)
);

--
-- Table structure for table `pollanswers`
--

DROP TABLE IF EXISTS `pollanswers`;
CREATE TABLE `pollanswers` (
  `qid` bigint NOT NULL default '0',
  `aid` bigint NOT NULL default '0',
  `answer` varchar(255) default NULL,
  `votes` bigint default NULL,
  PRIMARY KEY  (`qid`,`aid`)
);

--
-- Table structure for table `pollquestions`
--

DROP TABLE IF EXISTS `pollquestions`;
CREATE TABLE `pollquestions` (
  `qid` bigint NOT NULL auto_increment,
  `question` varchar(255) NOT NULL default '',
  `voters` bigint default NULL,
  `topic` int NOT NULL,
  `discussion` bigint NOT NULL default '0',
  `date` datetime default NULL,
  `uid` bigint NOT NULL default '0',
  `primaryskid` int default NULL,
  `autopoll` enum('no','yes') NOT NULL default 'no',
  `flags` enum('ok','delete','dirty') NOT NULL default 'ok',
  `polltype` enum('nodisplay','section','story') default 'section',
  PRIMARY KEY  (`qid`),
  KEY `uid` (`uid`),
  KEY `discussion` (`discussion`),
  KEY `ibfk_converttid_2` (`topic`)
);

--
-- Table structure for table `pollvoters`
--

DROP TABLE IF EXISTS `pollvoters`;
CREATE TABLE `pollvoters` (
  `qid` bigint NOT NULL default '0',
  `id` varchar(35) NOT NULL default '',
  `time` datetime default NULL,
  `uid` bigint NOT NULL default '0',
  KEY `qid` (`qid`,`id`,`uid`)
);

--
-- Table structure for table `preview`
--

DROP TABLE IF EXISTS `preview`;
CREATE TABLE `preview` (
  `preview_id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL,
  `src_fhid` bigint NOT NULL default '0',
  `preview_fhid` bigint NOT NULL default '0',
  `introtext` text NOT NULL,
  `bodytext` text NOT NULL,
  `active` enum('no','yes') default 'yes',
  `session` varchar(20) NOT NULL default '',
  `createtime` datetime NOT NULL default '1970-01-01 00:00:00',
  `title` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`preview_id`),
  KEY `uid` (`uid`),
  KEY `session` (`session`)
);

--
-- Table structure for table `preview_param`
--

DROP TABLE IF EXISTS `preview_param`;
CREATE TABLE `preview_param` (
  `param_id` bigint NOT NULL auto_increment,
  `preview_id` bigint NOT NULL,
  `name` varchar(32) NOT NULL default '',
  `value` text NOT NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `submission_key` (`preview_id`,`name`)
);

--
-- Table structure for table `projects`
--

DROP TABLE IF EXISTS `projects`;
CREATE TABLE `projects` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `unixname` varchar(24) NOT NULL default '',
  `textname` varchar(64) NOT NULL default '',
  `url_id` int NOT NULL default '0',
  `createtime` datetime NOT NULL default '1970-01-01 00:00:00',
  `srcname` varchar(32) NOT NULL default '0',
  `description` text NOT NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `unixname` (`unixname`)
);

--
-- Table structure for table `related_links`
--

DROP TABLE IF EXISTS `related_links`;
CREATE TABLE `related_links` (
  `id` int NOT NULL auto_increment,
  `keyword` varchar(30) NOT NULL default '',
  `name` varchar(80) default NULL,
  `link` varchar(128) NOT NULL default '',
  PRIMARY KEY  (`id`),
  KEY `keyword` (`keyword`)
);

--
-- Table structure for table `related_stories`
--

DROP TABLE IF EXISTS `related_stories`;
CREATE TABLE `related_stories` (
  `id` bigint NOT NULL auto_increment,
  `stoid` bigint default '0',
  `rel_stoid` bigint default '0',
  `rel_sid` varchar(16) NOT NULL default '',
  `title` varchar(255) default '',
  `url` varchar(255) default '',
  `cid` int NOT NULL default '0',
  `ordernum` int NOT NULL default '0',
  `fhid` bigint NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `stoid` (`stoid`)
);

--
-- Table structure for table `remarks`
--

DROP TABLE IF EXISTS `remarks`;
CREATE TABLE `remarks` (
  `rid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `stoid` bigint NOT NULL default '0',
  `priority` int NOT NULL default '0',
  `time` datetime NOT NULL,
  `remark` varchar(255) default NULL,
  `type` enum('system','user') default 'user',
  PRIMARY KEY  (`rid`),
  KEY `uid` (`uid`),
  KEY `stoid` (`stoid`),
  KEY `time` (`time`),
  KEY `priority` (`priority`)
);

--
-- Table structure for table `reskey_failures`
--

DROP TABLE IF EXISTS `reskey_failures`;
CREATE TABLE `reskey_failures` (
  `rkid` int NOT NULL default '0',
  `failure` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`rkid`)
);

--
-- Table structure for table `reskey_hourlysalt`
--

DROP TABLE IF EXISTS `reskey_hourlysalt`;
CREATE TABLE `reskey_hourlysalt` (
  `ts` datetime NOT NULL,
  `salt` varchar(20) NOT NULL default '',
  UNIQUE KEY `ts` (`ts`)
);

--
-- Table structure for table `reskey_resource_checks`
--

DROP TABLE IF EXISTS `reskey_resource_checks`;
CREATE TABLE `reskey_resource_checks` (
  `rkrcid` int NOT NULL auto_increment,
  `rkrid` int NOT NULL default '0',
  `type` enum('create','touch','use','all') NOT NULL default 'create',
  `class` varchar(255) default NULL,
  `ordernum` int default '0',
  PRIMARY KEY  (`rkrcid`),
  UNIQUE KEY `rkrid_name` (`rkrid`,`type`,`class`)
);

--
-- Table structure for table `reskey_resources`
--

DROP TABLE IF EXISTS `reskey_resources`;
CREATE TABLE `reskey_resources` (
  `rkrid` int NOT NULL auto_increment,
  `name` varchar(64) default NULL,
  `static` enum('yes','no') NOT NULL default 'no',
  PRIMARY KEY  (`rkrid`)
);

--
-- Table structure for table `reskey_sessions`
--

DROP TABLE IF EXISTS `reskey_sessions`;
CREATE TABLE `reskey_sessions` (
  `sessid` int NOT NULL auto_increment,
  `reskey` varchar(20) NOT NULL default '',
  `name` varchar(48) NOT NULL default '',
  `value` text,
  PRIMARY KEY  (`sessid`),
  UNIQUE KEY `reskey_name` (`reskey`,`name`),
  KEY `reskey` (`reskey`)
);

--
-- Table structure for table `reskey_vars`
--

DROP TABLE IF EXISTS `reskey_vars`;
CREATE TABLE `reskey_vars` (
  `rkrid` int NOT NULL default '0',
  `name` varchar(48) NOT NULL default '',
  `value` text,
  `description` varchar(255) default NULL,
  UNIQUE KEY `name_rkrid` (`name`,`rkrid`)
);

--
-- Table structure for table `reskeys`
--

DROP TABLE IF EXISTS `reskeys`;
CREATE TABLE `reskeys` (
  `rkid` int NOT NULL auto_increment,
  `reskey` varchar(20) NOT NULL default '',
  `rkrid` int NOT NULL default '0',
  `uid` bigint NOT NULL default '0',
  `srcid_ip` bigint NOT NULL default '0',
  `failures` int NOT NULL default '0',
  `touches` int NOT NULL default '0',
  `is_alive` enum('yes','no') NOT NULL default 'yes',
  `create_ts` datetime NOT NULL,
  `last_ts` datetime NOT NULL,
  `submit_ts` datetime default NULL,
  PRIMARY KEY  (`rkid`),
  UNIQUE KEY `reskey` (`reskey`),
  KEY `rkrid` (`rkrid`),
  KEY `uid` (`uid`),
  KEY `srcid_ip` (`srcid_ip`),
  KEY `create_ts` (`create_ts`),
  KEY `last_ts` (`last_ts`),
  KEY `submit_ts` (`submit_ts`)
);

--
-- Table structure for table `rss_raw`
--

DROP TABLE IF EXISTS `rss_raw`;
CREATE TABLE `rss_raw` (
  `id` bigint NOT NULL auto_increment,
  `link_signature` varchar(32) NOT NULL default '',
  `title_signature` varchar(32) NOT NULL default '',
  `description_signature` varchar(32) NOT NULL default '',
  `link` varchar(255) NOT NULL default '',
  `title` varchar(255) NOT NULL default '',
  `description` text,
  `subid` bigint default NULL,
  `bid` varchar(30) default NULL,
  `created` datetime default NULL,
  `processed` enum('no','yes') NOT NULL default 'no',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `uber_signature` (`link_signature`,`title_signature`,`description_signature`),
  KEY `processed` (`processed`)
);

--
-- Table structure for table `search_index_dump`
--

DROP TABLE IF EXISTS `search_index_dump`;
CREATE TABLE `search_index_dump` (
  `iid` int NOT NULL auto_increment,
  `id` int NOT NULL,
  `type` varchar(32) NOT NULL default '',
  `status` enum('new','changed','deleted') NOT NULL default 'new',
  PRIMARY KEY  (`iid`)
);

--
-- Table structure for table `section_extras`
--

DROP TABLE IF EXISTS `section_extras`;
CREATE TABLE `section_extras` (
  `param_id` bigint NOT NULL auto_increment,
  `section` varchar(30) NOT NULL default '',
  `name` varchar(100) NOT NULL default '',
  `value` varchar(100) NOT NULL default '',
  `type` enum('text','list','topics') NOT NULL default 'text',
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `extra` (`section`,`name`)
);

--
-- Table structure for table `section_subsections`
--

DROP TABLE IF EXISTS `section_subsections`;
CREATE TABLE `section_subsections` (
  `section` varchar(30) NOT NULL default '',
  `subsection` int NOT NULL default '0',
  PRIMARY KEY  (`section`,`subsection`)
);

--
-- Table structure for table `section_topics`
--

DROP TABLE IF EXISTS `section_topics`;
CREATE TABLE `section_topics` (
  `section` varchar(30) NOT NULL default '',
  `tid` int NOT NULL default '0',
  `type` varchar(16) NOT NULL default 'topic_1',
  PRIMARY KEY  (`section`,`type`,`tid`)
);

--
-- Table structure for table `sections`
--

DROP TABLE IF EXISTS `sections`;
CREATE TABLE `sections` (
  `id` int NOT NULL auto_increment,
  `section` varchar(30) NOT NULL default '',
  `artcount` bigint NOT NULL default '30',
  `title` varchar(64) NOT NULL default '',
  `qid` bigint NOT NULL default '0',
  `issue` int NOT NULL default '0',
  `url` varchar(32) NOT NULL default '',
  `hostname` varchar(32) NOT NULL default '',
  `index_handler` varchar(30) NOT NULL default 'index.pl',
  `writestatus` enum('ok','dirty') NOT NULL default 'ok',
  `type` enum('contained','collected') NOT NULL default 'contained',
  `rewrite` bigint NOT NULL default '3600',
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `defaultdisplaystatus` int NOT NULL default '0',
  `defaulttopic` int NOT NULL default '1',
  `defaultsection` varchar(30) default NULL,
  `defaultsubsection` int NOT NULL default '0',
  `defaultcommentstatus` enum('disabled','enabled','friends_only','friends_fof_only','no_foe','no_foe_eof') NOT NULL default 'enabled',
  `cookiedomain` varchar(128) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `section` (`section`)
);

--
-- Table structure for table `sections_contained`
--

DROP TABLE IF EXISTS `sections_contained`;
CREATE TABLE `sections_contained` (
  `id` int NOT NULL auto_increment,
  `container` varchar(30) NOT NULL default '',
  `section` varchar(30) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `container` (`container`,`section`)
);

--
-- Table structure for table `sessions`
--

DROP TABLE IF EXISTS `sessions`;
CREATE TABLE `sessions` (
  `session` bigint NOT NULL auto_increment,
  `uid` bigint default NULL,
  `lasttime` datetime default NULL,
  `lasttitle` varchar(50) default NULL,
  `last_sid` varchar(16) default NULL,
  `last_subid` bigint default NULL,
  `last_fhid` bigint default NULL,
  `last_action` varchar(16) default NULL,
  PRIMARY KEY  (`session`),
  UNIQUE KEY `uid` (`uid`)
);

--
-- Table structure for table `shifts`
--

DROP TABLE IF EXISTS `shifts`;
CREATE TABLE `shifts` (
  `date` datetime default NULL,
  `uid` bigint default NULL,
  `type` enum('shift','default') default NULL,
  `shift` enum('morning','afternoon','evening') default NULL,
  `last_changed` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  KEY `byuser` (`uid`),
  KEY `bytime` (`last_changed`),
  KEY `byshift` (`shift`,`uid`,`type`)
);

--
-- Table structure for table `shill_ids`
--

DROP TABLE IF EXISTS `shill_ids`;
CREATE TABLE `shill_ids` (
  `shill_id` int NOT NULL default '0',
  `user` varchar(16) NOT NULL default '',
  `extra` varchar(40) NOT NULL default '',
  `skid` int NOT NULL default '0',
  PRIMARY KEY  (`shill_id`)
);

--
-- Table structure for table `signoff`
--

DROP TABLE IF EXISTS `signoff`;
CREATE TABLE `signoff` (
  `soid` bigint NOT NULL auto_increment,
  `stoid` bigint NOT NULL default '0',
  `uid` bigint NOT NULL default '0',
  `signoff_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `signoff_type` varchar(16) NOT NULL default '',
  PRIMARY KEY  (`soid`),
  KEY `stoid` (`stoid`)
);

--
-- Table structure for table `site_info`
--

DROP TABLE IF EXISTS `site_info`;
CREATE TABLE `site_info` (
  `param_id` bigint NOT NULL auto_increment,
  `name` varchar(50) NOT NULL default '',
  `value` varchar(200) NOT NULL default '',
  `description` varchar(255) default NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `site_keys` (`name`,`value`)
);

--
-- Table structure for table `skin_colors`
--

DROP TABLE IF EXISTS `skin_colors`;
CREATE TABLE `skin_colors` (
  `skid` int NOT NULL default '0',
  `name` varchar(24) NOT NULL default '',
  `skincolor` varchar(12) NOT NULL default '',
  UNIQUE KEY `skid_name` (`skid`,`name`)
);

--
-- Table structure for table `skins`
--

DROP TABLE IF EXISTS `skins`;
CREATE TABLE `skins` (
  `skid` int NOT NULL auto_increment,
  `nexus` int NOT NULL,
  `artcount_min` bigint NOT NULL default '10',
  `artcount_max` bigint NOT NULL default '30',
  `name` varchar(30) NOT NULL default '',
  `othername` varchar(30) NOT NULL default '',
  `title` varchar(64) NOT NULL default '',
  `issue` enum('no','yes') NOT NULL default 'no',
  `submittable` enum('no','yes') NOT NULL default 'yes',
  `searchable` enum('no','yes') NOT NULL default 'yes',
  `storypickable` enum('no','yes') NOT NULL default 'yes',
  `skinindex` enum('no','yes') NOT NULL default 'yes',
  `url` varchar(255) NOT NULL default '',
  `hostname` varchar(128) NOT NULL default '',
  `cookiedomain` varchar(128) NOT NULL default '',
  `index_handler` varchar(30) NOT NULL default 'index.pl',
  `max_rewrite_secs` bigint NOT NULL default '3600',
  `last_rewrite` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `ac_uid` bigint NOT NULL default '0',
  `older_stories_max` bigint NOT NULL default '0',
  `require_acl` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`skid`),
  UNIQUE KEY `name` (`name`),
  KEY `ibfk_converttid_3` (`nexus`)
);

--
-- Table structure for table `slashd_errnotes`
--

DROP TABLE IF EXISTS `slashd_errnotes`;
CREATE TABLE `slashd_errnotes` (
  `ts` datetime NOT NULL,
  `taskname` varchar(50) NOT NULL default 'SLASHD',
  `line` bigint NOT NULL default '0',
  `errnote` varchar(255) NOT NULL default '',
  `moreinfo` text,
  KEY `ts` (`ts`),
  KEY `taskname_ts` (`taskname`,`ts`)
);

--
-- Table structure for table `slashd_status`
--

DROP TABLE IF EXISTS `slashd_status`;
CREATE TABLE `slashd_status` (
  `task` varchar(50) NOT NULL default '',
  `hostname_regex` varchar(2048) NOT NULL default '',
  `next_begin` datetime default NULL,
  `in_progress` int NOT NULL default '0',
  `last_completed` datetime default NULL,
  `summary` varchar(255) NOT NULL default '',
  `duration` float(6,2) NOT NULL default '0.00',
  PRIMARY KEY  (`task`)
);

--
-- Table structure for table `soap_methods`
--

DROP TABLE IF EXISTS `soap_methods`;
CREATE TABLE `soap_methods` (
  `id` bigint NOT NULL auto_increment,
  `class` varchar(100) NOT NULL default '',
  `method` varchar(100) NOT NULL default '',
  `seclev` bigint NOT NULL default '1000',
  `subscriber_only` int NOT NULL default '0',
  `formkeys` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `soap_method` (`class`,`method`)
);

--
-- Table structure for table `spamarmors`
--

DROP TABLE IF EXISTS `spamarmors`;
CREATE TABLE `spamarmors` (
  `armor_id` bigint NOT NULL auto_increment,
  `name` varchar(40) default NULL,
  `code` text,
  `active` bigint default '1',
  PRIMARY KEY  (`armor_id`)
);

--
-- Table structure for table `sphinx_counter`
--

DROP TABLE IF EXISTS `sphinx_counter`;
CREATE TABLE `sphinx_counter` (
  `src` int NOT NULL,
  `completion` int default NULL,
  `last_seen` datetime NOT NULL,
  `started` datetime NOT NULL,
  `elapsed` int default NULL,
  UNIQUE KEY `src_completion` (`src`,`completion`)
);

--
-- Table structure for table `sphinx_counter_archived`
--

DROP TABLE IF EXISTS `sphinx_counter_archived`;
CREATE TABLE `sphinx_counter_archived` (
  `src` int NOT NULL,
  `completion` int NOT NULL,
  `last_seen` datetime NOT NULL,
  `started` datetime NOT NULL,
  `elapsed` int default NULL,
  UNIQUE KEY `src_completion` (`src`,`completion`)
);

--
-- Table structure for table `sphinx_index`
--

DROP TABLE IF EXISTS `sphinx_index`;
CREATE TABLE `sphinx_index` (
  `src` int NOT NULL,
  `name` varchar(48) NOT NULL,
  `asynch` int NOT NULL default '1',
  `laststart` datetime NOT NULL default '2000-01-01 00:00:00',
  `frequency` int NOT NULL default '86400',
  PRIMARY KEY  (`src`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `sphinx_search`
--

DROP TABLE IF EXISTS `sphinx_search`;
CREATE TABLE `sphinx_search` (
  `globjid` int NOT NULL,
  `weight` int NOT NULL,
  `query` varchar(3072) NOT NULL,
  `_sph_count` int NOT NULL,
  KEY `query` (`query`(767))
);

--
-- Table structure for table `static_files`
--

DROP TABLE IF EXISTS `static_files`;
CREATE TABLE `static_files` (
  `sfid` bigint NOT NULL auto_increment,
  `stoid` bigint NOT NULL,
  `filetype` enum('file','image','audio') NOT NULL default 'file',
  `name` varchar(255) NOT NULL default '',
  `width` int NOT NULL default '0',
  `height` int NOT NULL default '0',
  `fhid` bigint NOT NULL,
  PRIMARY KEY  (`sfid`),
  KEY `stoid` (`stoid`)
);

--
-- Table structure for table `stats_daily`
--

DROP TABLE IF EXISTS `stats_daily`;
CREATE TABLE `stats_daily` (
  `id` int NOT NULL auto_increment,
  `skid` int NOT NULL default '0',
  `day` date NOT NULL,
  `name` varchar(48) NOT NULL default '',
  `value` float NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `day_key_pair` (`day`,`name`,`skid`),
  UNIQUE KEY `skid_day_name` (`skid`,`day`,`name`),
  KEY `name_day` (`name`,`day`)
);

--
-- Table structure for table `stats_graphs_index`
--

DROP TABLE IF EXISTS `stats_graphs_index`;
CREATE TABLE `stats_graphs_index` (
  `day` date NOT NULL,
  `md5` varchar(32) NOT NULL default '',
  `id` blob
);

--
-- Table structure for table `stories`
--

DROP TABLE IF EXISTS `stories`;
CREATE TABLE `stories` (
  `stoid` bigint NOT NULL auto_increment,
  `sid` varchar(16) NOT NULL default '',
  `uid` bigint NOT NULL default '0',
  `dept` varchar(100) default NULL,
  `time` datetime NOT NULL,
  `hits` bigint NOT NULL default '0',
  `discussion` bigint default NULL,
  `primaryskid` int default NULL,
  `tid` int default NULL,
  `submitter` bigint NOT NULL default '0',
  `commentcount` int NOT NULL default '0',
  `hitparade` varchar(64) NOT NULL default '0,0,0,0,0,0,0',
  `writestatus` enum('ok','delete','dirty','archived') NOT NULL default 'ok',
  `is_archived` enum('no','yes') NOT NULL default 'no',
  `in_trash` enum('no','yes') NOT NULL default 'no',
  `day_published` date NOT NULL,
  `qid` bigint default NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `body_length` bigint NOT NULL default '0',
  `word_count` bigint NOT NULL default '0',
  `archive_last_update` datetime NOT NULL default '1970-01-01 00:00:00',
  PRIMARY KEY  (`stoid`),
  UNIQUE KEY `sid` (`sid`),
  KEY `uid` (`uid`),
  KEY `is_archived` (`is_archived`),
  KEY `time` (`time`),
  KEY `submitter` (`submitter`),
  KEY `day_published` (`day_published`),
  KEY `skidtid` (`primaryskid`,`tid`),
  KEY `discussion_stoid` (`discussion`,`stoid`),
  KEY `ibfk_converttid_4` (`tid`)
);

--
-- Table structure for table `stories_media`
--

DROP TABLE IF EXISTS `stories_media`;
CREATE TABLE `stories_media` (
  `smid` int NOT NULL auto_increment,
  `sid` varchar(16) default NULL,
  `stoid` bigint default '0',
  `type` enum('image','audio') default NULL,
  `width` int default NULL,
  `height` int default NULL,
  `location` varchar(255) default NULL,
  `name` varchar(255) default NULL,
  PRIMARY KEY  (`smid`)
);

--
-- Table structure for table `story_dirty`
--

DROP TABLE IF EXISTS `story_dirty`;
CREATE TABLE `story_dirty` (
  `stoid` bigint NOT NULL default '0',
  PRIMARY KEY  (`stoid`)
);

--
-- Table structure for table `story_files`
--

DROP TABLE IF EXISTS `story_files`;
CREATE TABLE `story_files` (
  `id` int NOT NULL auto_increment,
  `stoid` bigint NOT NULL default '0',
  `description` varchar(80) NOT NULL default '',
  `file_id` varchar(32) NOT NULL default '',
  `isimage` enum('no','yes') NOT NULL default 'no',
  PRIMARY KEY  (`id`),
  KEY `stoid` (`stoid`),
  KEY `file_id` (`file_id`)
);

--
-- Table structure for table `story_param`
--

DROP TABLE IF EXISTS `story_param`;
CREATE TABLE `story_param` (
  `param_id` bigint NOT NULL auto_increment,
  `stoid` bigint NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `value` text NOT NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `story_key` (`stoid`,`name`)
);

--
-- Table structure for table `story_render_dirty`
--

DROP TABLE IF EXISTS `story_render_dirty`;
CREATE TABLE `story_render_dirty` (
  `stoid` bigint NOT NULL default '0',
  PRIMARY KEY  (`stoid`)
);

--
-- Table structure for table `story_text`
--

DROP TABLE IF EXISTS `story_text`;
CREATE TABLE `story_text` (
  `stoid` bigint NOT NULL default '0',
  `title` varchar(100) NOT NULL default '',
  `introtext` text,
  `bodytext` text,
  `relatedtext` text,
  `rendered` text,
  PRIMARY KEY  (`stoid`)
);

--
-- Table structure for table `story_topics_chosen`
--

DROP TABLE IF EXISTS `story_topics_chosen`;
CREATE TABLE `story_topics_chosen` (
  `stoid` bigint NOT NULL default '0',
  `tid` int NOT NULL,
  `weight` float NOT NULL default '1',
  UNIQUE KEY `story_topic` (`stoid`,`tid`),
  KEY `tid_stoid` (`tid`,`stoid`)
);

--
-- Table structure for table `story_topics_rendered`
--

DROP TABLE IF EXISTS `story_topics_rendered`;
CREATE TABLE `story_topics_rendered` (
  `stoid` bigint NOT NULL default '0',
  `tid` int NOT NULL,
  UNIQUE KEY `story_topic` (`stoid`,`tid`),
  KEY `tid_stoid` (`tid`,`stoid`)
);

--
-- Table structure for table `string_param`
--

DROP TABLE IF EXISTS `string_param`;
CREATE TABLE `string_param` (
  `param_id` int NOT NULL auto_increment,
  `type` varchar(32) NOT NULL default '',
  `code` varchar(128) NOT NULL default '',
  `name` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `code_key` (`type`,`code`)
);

--
-- Table structure for table `submission_param`
--

DROP TABLE IF EXISTS `submission_param`;
CREATE TABLE `submission_param` (
  `param_id` bigint NOT NULL auto_increment,
  `subid` bigint NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `value` text NOT NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `submission_key` (`subid`,`name`)
);

--
-- Table structure for table `submissions`
--

DROP TABLE IF EXISTS `submissions`;
CREATE TABLE `submissions` (
  `subid` bigint NOT NULL auto_increment,
  `email` varchar(255) NOT NULL default '',
  `emaildomain` varchar(255) NOT NULL default '',
  `name` varchar(50) NOT NULL default '',
  `time` datetime NOT NULL,
  `subj` varchar(50) NOT NULL default '',
  `story` text NOT NULL,
  `tid` int NOT NULL,
  `note` varchar(30) NOT NULL default '',
  `primaryskid` int default NULL,
  `comment` varchar(255) NOT NULL default '',
  `uid` bigint NOT NULL default '0',
  `ipid` varchar(32) NOT NULL default '',
  `subnetid` varchar(32) NOT NULL default '',
  `del` int NOT NULL default '0',
  `weight` float NOT NULL default '0',
  `signature` varchar(32) NOT NULL default '',
  `mediatype` enum('none','text','video','image','audio') NOT NULL default 'none',
  PRIMARY KEY  (`subid`),
  UNIQUE KEY `signature` (`signature`),
  KEY `del` (`del`),
  KEY `uid` (`uid`),
  KEY `ipid` (`ipid`),
  KEY `subnetid` (`subnetid`),
  KEY `primaryskid_tid` (`primaryskid`,`tid`),
  KEY `tid` (`tid`),
  KEY `emaildomain` (`emaildomain`),
  KEY `time_emaildomain` (`time`,`emaildomain`)
);

--
-- Table structure for table `submissions_notes`
--

DROP TABLE IF EXISTS `submissions_notes`;
CREATE TABLE `submissions_notes` (
  `noid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `submatch` varchar(32) NOT NULL default '',
  `subnote` text,
  `time` datetime default NULL,
  PRIMARY KEY  (`noid`)
);

--
-- Table structure for table `subscribe_payments`
--

DROP TABLE IF EXISTS `subscribe_payments`;
CREATE TABLE `subscribe_payments` (
  `spid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `email` varchar(255) NOT NULL default '',
  `ts` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `payment_gross` decimal(10,2) NOT NULL default '0.00',
  `payment_net` decimal(10,2) NOT NULL default '0.00',
  `pages` bigint NOT NULL default '0',
  `transaction_id` varchar(255) NOT NULL default '',
  `method` varchar(6) default NULL,
  `memo` varchar(255) NOT NULL default '',
  `data` blob,
  `payment_type` varchar(10) default 'user',
  `puid` bigint default NULL,
  PRIMARY KEY  (`spid`),
  UNIQUE KEY `transaction_id` (`transaction_id`),
  KEY `uid` (`uid`),
  KEY `ts` (`ts`),
  KEY `puid` (`puid`)
);

--
-- Table structure for table `subsections`
--

DROP TABLE IF EXISTS `subsections`;
CREATE TABLE `subsections` (
  `id` int NOT NULL auto_increment,
  `title` varchar(30) NOT NULL default '',
  `artcount` bigint NOT NULL default '30',
  `alttext` varchar(40) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `code_key` (`title`)
);

--
-- Table structure for table `surv_answers`
--

DROP TABLE IF EXISTS `surv_answers`;
CREATE TABLE `surv_answers` (
  `sqaid` bigint NOT NULL auto_increment,
  `svid` bigint NOT NULL default '0',
  `sqid` bigint NOT NULL default '0',
  `sqcid` bigint NOT NULL default '0',
  `karma` bigint default NULL,
  `owneruid` bigint NOT NULL default '0',
  `ipaddress` varchar(35) default NULL,
  `answer` varchar(255) NOT NULL default '',
  `datetimecreated` datetime NOT NULL,
  `tokens` bigint default NULL,
  `subnetid` varchar(32) default '',
  `ipid` varchar(35) default NULL,
  PRIMARY KEY  (`sqaid`),
  KEY `own` (`owneruid`),
  KEY `karma` (`karma`),
  KEY `svid` (`svid`),
  KEY `sqid` (`sqid`),
  KEY `sqcid` (`sqcid`),
  KEY `bigboy` (`sqaid`,`owneruid`,`ipaddress`),
  KEY `didthey` (`svid`,`sqid`,`owneruid`,`ipaddress`)
);

--
-- Table structure for table `surv_answers_params`
--

DROP TABLE IF EXISTS `surv_answers_params`;
CREATE TABLE `surv_answers_params` (
  `sqaid` bigint NOT NULL default '0',
  `param_id` bigint NOT NULL default '0',
  `value` varchar(255) default NULL,
  PRIMARY KEY  (`sqaid`,`param_id`)
);

--
-- Table structure for table `surv_choices`
--

DROP TABLE IF EXISTS `surv_choices`;
CREATE TABLE `surv_choices` (
  `sqcid` bigint NOT NULL auto_increment,
  `sqid` bigint NOT NULL default '0',
  `ordnum` int NOT NULL default '0',
  `name` varchar(255) NOT NULL default '',
  `count` bigint NOT NULL default '0',
  `datetimecreated` datetime NOT NULL,
  `datetimeupdated` datetime NOT NULL,
  PRIMARY KEY  (`sqcid`),
  KEY `ordnum` (`ordnum`),
  KEY `sqid` (`sqid`)
);

--
-- Table structure for table `surv_questions`
--

DROP TABLE IF EXISTS `surv_questions`;
CREATE TABLE `surv_questions` (
  `sqid` bigint NOT NULL auto_increment,
  `svid` bigint NOT NULL default '0',
  `next_sqid` bigint default NULL,
  `condnext_sqid` bigint default NULL,
  `condnext_sqcid` bigint default NULL,
  `ordnum` int default '1',
  `description` varchar(255) default '',
  `type` enum('single_choice_checkboxlist','single_choice_radio','single_choice_pulldown','multi_choice_checkboxlist','multi_choice_pulldown','fill_in_the_blank') default NULL,
  `datetimecreated` datetime,
  `datetimeupdated` datetime,
  PRIMARY KEY  (`sqid`),
  KEY `ordnum` (`ordnum`),
  KEY `svid` (`svid`),
  KEY `squidsvid` (`sqid`,`svid`),
  KEY `svidordnum` (`svid`,`ordnum`),
  KEY `next_sqid` (`next_sqid`)
);

--
-- Table structure for table `surv_surveys`
--

DROP TABLE IF EXISTS `surv_surveys`;
CREATE TABLE `surv_surveys` (
  `svid` bigint NOT NULL auto_increment,
  `ispublished` enum('yes','no') default 'no',
  `requirement` enum('','anon','loggedin','hasmoderated','hasposted','uidrange','seclev','acl_read','acl_write') default '',
  `reqval` varchar(32) default '',
  `owneruid` int NOT NULL default '0',
  `qcount` int default '0',
  `skid` int default NULL,
  `tid` int default NULL,
  `acl_read` varchar(32) default NULL,
  `seclev` bigint default NULL,
  `discussionid` bigint default NULL,
  `datetimecreated` datetime,
  `datetimeupdated` datetime,
  `datetimeopenned` datetime,
  `datetimeopens` datetime,
  `datetimeclosed` datetime,
  `datetimeexpires` datetime,
  `name` varchar(150) NOT NULL default '',
  `description` text,
  `svsid` varchar(16) NOT NULL default '',
  `stoid` bigint default NULL,
  `thankyou` text,
  `uid_min` bigint default NULL,
  `uid_max` bigint default NULL,
  PRIMARY KEY  (`svid`),
  UNIQUE KEY `svsid` (`svsid`),
  KEY `skid` (`skid`),
  KEY `owneruid` (`owneruid`),
  KEY `acl_read` (`acl_read`),
  KEY `ispub` (`ispublished`),
  KEY `req` (`requirement`),
  KEY `seclev` (`seclev`),
  KEY `stoid` (`stoid`),
  KEY `tid` (`tid`)
);

--
-- Table structure for table `surv_surveys_params`
--

DROP TABLE IF EXISTS `surv_surveys_params`;
CREATE TABLE `surv_surveys_params` (
  `param_id` bigint NOT NULL auto_increment,
  `svid` bigint NOT NULL default '0',
  `name` varchar(30) NOT NULL default '',
  `value` text,
  PRIMARY KEY  (`param_id`),
  KEY `svid` (`svid`)
);

--
-- Table structure for table `tag_params`
--

DROP TABLE IF EXISTS `tag_params`;
CREATE TABLE `tag_params` (
  `tagid` int NOT NULL,
  `name` varchar(32) NOT NULL default '',
  `value` varchar(64) NOT NULL default '',
  UNIQUE KEY `tag_name` (`tagid`,`name`)
);

--
-- Table structure for table `tagbox_metamod_history`
--

DROP TABLE IF EXISTS `tagbox_metamod_history`;
CREATE TABLE `tagbox_metamod_history` (
  `globjid` int NOT NULL,
  `max_tagid_seen` int NOT NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`globjid`)
);

--
-- Table structure for table `tagboxes`
--

DROP TABLE IF EXISTS `tagboxes`;
CREATE TABLE `tagboxes` (
  `tbid` int NOT NULL auto_increment,
  `name` varchar(32) NOT NULL default '',
  `weight` float NOT NULL default '1',
  `last_tagid_logged` int NOT NULL,
  `last_run_completed` datetime default NULL,
  `last_tdid_logged` int NOT NULL,
  `last_tuid_logged` int NOT NULL,
  PRIMARY KEY  (`tbid`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `tagboxlog_feeder`
--

DROP TABLE IF EXISTS `tagboxlog_feeder`;
CREATE TABLE `tagboxlog_feeder` (
  `tfid` int NOT NULL auto_increment,
  `created_at` datetime NOT NULL,
  `tbid` int NOT NULL,
  `affected_id` int NOT NULL,
  `importance` float NOT NULL default '1',
  `claimed` datetime default NULL,
  `tagid` int default NULL,
  `tdid` int default NULL,
  `tuid` int default NULL,
  PRIMARY KEY  (`tfid`),
  KEY `tbid_tagid` (`tbid`,`tagid`),
  KEY `tbid_affectedid` (`tbid`,`affected_id`),
  KEY `tbid_tdid` (`tbid`,`tdid`),
  KEY `tbid_tuid` (`tbid`,`tuid`)
);

--
-- Table structure for table `tagboxlog_feeder_archived`
--

DROP TABLE IF EXISTS `tagboxlog_feeder_archived`;
CREATE TABLE `tagboxlog_feeder_archived` (
  `tfid` int NOT NULL auto_increment,
  `created_at` datetime NOT NULL,
  `tbid` int NOT NULL,
  `affected_id` int NOT NULL,
  `importance` float NOT NULL default '1',
  `claimed` datetime default NULL,
  `tagid` int default NULL,
  `tdid` int default NULL,
  `tuid` int default NULL,
  PRIMARY KEY  (`tfid`),
  KEY `tbid_tagid` (`tbid`,`tagid`),
  KEY `tbid_affectedid` (`tbid`,`affected_id`),
  KEY `tbid_tdid` (`tbid`,`tdid`),
  KEY `tbid_tuid` (`tbid`,`tuid`)
);

--
-- Table structure for table `tagcommand_adminlog`
--

DROP TABLE IF EXISTS `tagcommand_adminlog`;
CREATE TABLE `tagcommand_adminlog` (
  `id` int NOT NULL auto_increment,
  `cmdtype` varchar(6) NOT NULL,
  `tagnameid` int NOT NULL,
  `globjid` int default NULL,
  `adminuid` bigint NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `created_at` (`created_at`),
  KEY `tagnameid_globjid` (`tagnameid`,`globjid`)
);

--
-- Table structure for table `tagname_cache`
--

DROP TABLE IF EXISTS `tagname_cache`;
CREATE TABLE `tagname_cache` (
  `tagnameid` int NOT NULL,
  `tagname` varchar(64) NOT NULL,
  `weight` float NOT NULL default '0',
  PRIMARY KEY  (`tagnameid`),
  UNIQUE KEY `tagname` (`tagname`)
);

--
-- Table structure for table `tagname_params`
--

DROP TABLE IF EXISTS `tagname_params`;
CREATE TABLE `tagname_params` (
  `tagnameid` int NOT NULL default '0',
  `name` varchar(32) NOT NULL default '',
  `value` varchar(64) NOT NULL default '',
  UNIQUE KEY `tagname_name` (`tagnameid`,`name`),
  KEY `name` (`name`)
);

--
-- Table structure for table `tagnames`
--

DROP TABLE IF EXISTS `tagnames`;
CREATE TABLE `tagnames` (
  `tagnameid` int NOT NULL auto_increment,
  `tagname` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`tagnameid`),
  UNIQUE KEY `tagname` (`tagname`)
);

--
-- Table structure for table `tagnames_similarity_rendered`
--

DROP TABLE IF EXISTS `tagnames_similarity_rendered`;
CREATE TABLE `tagnames_similarity_rendered` (
  `clid` int NOT NULL default '0',
  `syn_tnid` int NOT NULL default '0',
  `similarity` enum('1','-1') NOT NULL default '1',
  `pref_tnid` int NOT NULL default '0',
  UNIQUE KEY `clid_syn_sim` (`clid`,`syn_tnid`,`similarity`)
);

--
-- Table structure for table `tagnames_synonyms_chosen`
--

DROP TABLE IF EXISTS `tagnames_synonyms_chosen`;
CREATE TABLE `tagnames_synonyms_chosen` (
  `clid` int NOT NULL default '0',
  `pref_tnid` int NOT NULL default '0',
  `syn_tnid` int NOT NULL default '0',
  UNIQUE KEY `clid_pref_syn` (`clid`,`pref_tnid`,`syn_tnid`)
);

--
-- Table structure for table `tags`
--

DROP TABLE IF EXISTS `tags`;
CREATE TABLE `tags` (
  `tagid` int NOT NULL auto_increment,
  `tagnameid` int NOT NULL default '0',
  `globjid` int NOT NULL default '0',
  `uid` bigint NOT NULL default '0',
  `created_at` datetime NOT NULL,
  `inactivated` datetime default NULL,
  `private` enum('yes','no') NOT NULL default 'no',
  PRIMARY KEY  (`tagid`),
  KEY `tagnameid` (`tagnameid`),
  KEY `globjid_tagnameid` (`globjid`,`tagnameid`),
  KEY `created_at` (`created_at`),
  KEY `uid_tagnameid_globjid_inactivated` (`uid`,`tagnameid`,`globjid`,`inactivated`)
);

--
-- Table structure for table `tags_dayofweek`
--

DROP TABLE IF EXISTS `tags_dayofweek`;
CREATE TABLE `tags_dayofweek` (
  `day` int NOT NULL default '0',
  `proportion` float NOT NULL default '0',
  PRIMARY KEY  (`day`)
);

--
-- Table structure for table `tags_deactivated`
--

DROP TABLE IF EXISTS `tags_deactivated`;
CREATE TABLE `tags_deactivated` (
  `tdid` int NOT NULL auto_increment,
  `tagid` int NOT NULL,
  PRIMARY KEY  (`tdid`),
  KEY `tagid` (`tagid`)
);

--
-- Table structure for table `tags_hourofday`
--

DROP TABLE IF EXISTS `tags_hourofday`;
CREATE TABLE `tags_hourofday` (
  `hour` int NOT NULL default '0',
  `proportion` float NOT NULL default '0',
  PRIMARY KEY  (`hour`)
);

--
-- Table structure for table `tags_peerclout`
--

DROP TABLE IF EXISTS `tags_peerclout`;
CREATE TABLE `tags_peerclout` (
  `tpcid` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `clid` int NOT NULL,
  `gen` int NOT NULL default '0',
  `clout` float NOT NULL default '0',
  PRIMARY KEY  (`tpcid`),
  UNIQUE KEY `uid_clid` (`uid`,`clid`),
  KEY `clid_gen_uid` (`clid`,`gen`,`uid`)
);

--
-- Table structure for table `tags_searched`
--

DROP TABLE IF EXISTS `tags_searched`;
CREATE TABLE `tags_searched` (
  `tseid` int NOT NULL auto_increment,
  `tagnameid` int NOT NULL,
  `searched_at` datetime NOT NULL,
  `uid` bigint default NULL,
  PRIMARY KEY  (`tseid`),
  KEY `tagnameid` (`tagnameid`),
  KEY `searched_at` (`searched_at`)
);

--
-- Table structure for table `tags_udc`
--

DROP TABLE IF EXISTS `tags_udc`;
CREATE TABLE `tags_udc` (
  `hourtime` datetime NOT NULL,
  `udc` float NOT NULL default '0',
  PRIMARY KEY  (`hourtime`)
);

--
-- Table structure for table `tags_userchange`
--

DROP TABLE IF EXISTS `tags_userchange`;
CREATE TABLE `tags_userchange` (
  `tuid` int NOT NULL auto_increment,
  `created_at` datetime NOT NULL,
  `uid` bigint NOT NULL,
  `user_key` varchar(32) NOT NULL,
  `value_old` text,
  `value_new` text,
  PRIMARY KEY  (`tuid`),
  KEY `uid` (`uid`)
);

--
-- Table structure for table `templates`
--

DROP TABLE IF EXISTS `templates`;
CREATE TABLE `templates` (
  `tpid` bigint NOT NULL auto_increment,
  `name` varchar(30) NOT NULL default '',
  `page` varchar(20) NOT NULL default 'misc',
  `skin` varchar(30) NOT NULL default 'default',
  `lang` varchar(5) NOT NULL default 'en_US',
  `template` text,
  `seclev` bigint NOT NULL default '0',
  `description` text,
  `title` varchar(128) default NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`tpid`),
  UNIQUE KEY `true_template` (`name`,`page`,`skin`,`lang`)
);

--
-- Table structure for table `topic_nexus`
--

DROP TABLE IF EXISTS `topic_nexus`;
CREATE TABLE `topic_nexus` (
  `tid` int NOT NULL,
  `current_qid` bigint default NULL,
  PRIMARY KEY  (`tid`)
);

--
-- Table structure for table `topic_nexus_dirty`
--

DROP TABLE IF EXISTS `topic_nexus_dirty`;
CREATE TABLE `topic_nexus_dirty` (
  `tid` int NOT NULL,
  PRIMARY KEY  (`tid`)
);

--
-- Table structure for table `topic_nexus_extras`
--

DROP TABLE IF EXISTS `topic_nexus_extras`;
CREATE TABLE `topic_nexus_extras` (
  `extras_id` bigint NOT NULL auto_increment,
  `tid` int NOT NULL,
  `extras_keyword` varchar(100) NOT NULL default '',
  `extras_textname` varchar(100) NOT NULL default '',
  `type` enum('text','list','textarea') NOT NULL default 'text',
  `content_type` enum('story','comment') NOT NULL default 'story',
  `required` enum('no','yes') NOT NULL default 'no',
  `ordering` int NOT NULL default '0',
  PRIMARY KEY  (`extras_id`),
  UNIQUE KEY `tid_keyword` (`tid`,`extras_keyword`)
);

--
-- Table structure for table `topic_param`
--

DROP TABLE IF EXISTS `topic_param`;
CREATE TABLE `topic_param` (
  `param_id` bigint NOT NULL auto_increment,
  `tid` int NOT NULL,
  `name` varchar(32) NOT NULL default '',
  `value` text NOT NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `topic_key` (`tid`,`name`)
);

--
-- Table structure for table `topic_parents`
--

DROP TABLE IF EXISTS `topic_parents`;
CREATE TABLE `topic_parents` (
  `tid` int NOT NULL,
  `parent_tid` int NOT NULL,
  `min_weight` float NOT NULL default '1',
  UNIQUE KEY `child_and_parent` (`tid`,`parent_tid`),
  KEY `parent_tid` (`parent_tid`)
);

--
-- Table structure for table `topics`
--

DROP TABLE IF EXISTS `topics`;
CREATE TABLE `topics` (
  `tid` int NOT NULL auto_increment,
  `keyword` varchar(20) NOT NULL default '',
  `textname` varchar(80) NOT NULL default '',
  `series` enum('no','yes') NOT NULL default 'no',
  `image` varchar(100) NOT NULL default '',
  `width` int NOT NULL default '0',
  `height` int NOT NULL default '0',
  `submittable` enum('no','yes') default 'yes',
  `searchable` enum('no','yes') NOT NULL default 'yes',
  `storypickable` enum('no','yes') NOT NULL default 'yes',
  `usesprite` enum('no','yes') NOT NULL default 'no',
  PRIMARY KEY  (`tid`),
  UNIQUE KEY `keyword` (`keyword`)
);

--
-- Table structure for table `topics_changetid`
--

DROP TABLE IF EXISTS `topics_changetid`;
CREATE TABLE `topics_changetid` (
  `tid_old` int NOT NULL,
  `tagnameid_new` int NOT NULL,
  PRIMARY KEY  (`tid_old`)
);

--
-- Table structure for table `tzcodes`
--

DROP TABLE IF EXISTS `tzcodes`;
CREATE TABLE `tzcodes` (
  `tz` varchar(4) NOT NULL default '',
  `off_set` bigint NOT NULL default '0',
  `description` varchar(64) default NULL,
  `dst_region` varchar(32) default NULL,
  `dst_tz` varchar(4) default NULL,
  `dst_off_set` bigint default NULL,
  PRIMARY KEY  (`tz`)
);

--
-- Table structure for table `uncommonstorywords`
--

DROP TABLE IF EXISTS `uncommonstorywords`;
CREATE TABLE `uncommonstorywords` (
  `word` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`word`)
);

--
-- Table structure for table `urls`
--

DROP TABLE IF EXISTS `urls`;
CREATE TABLE `urls` (
  `url_id` int NOT NULL auto_increment,
  `url_digest` varchar(32) NOT NULL,
  `url` text NOT NULL,
  `is_success` int default NULL,
  `createtime` datetime default NULL,
  `last_attempt` datetime default NULL,
  `last_success` datetime default NULL,
  `believed_fresh_until` datetime default NULL,
  `status_code` int default NULL,
  `reason_phrase` varchar(30) default NULL,
  `content_type` varchar(60) default NULL,
  `initialtitle` varchar(255) default NULL,
  `validatedtitle` varchar(255) default NULL,
  `tags_top` varchar(255) NOT NULL default '',
  `popularity` float NOT NULL default '0',
  `anon_bookmarks` bigint NOT NULL default '0',
  PRIMARY KEY  (`url_id`),
  UNIQUE KEY `url_digest` (`url_digest`),
  KEY `bfu` (`believed_fresh_until`)
);

--
-- Table structure for table `user_achievement_streaks`
--

DROP TABLE IF EXISTS `user_achievement_streaks`;
CREATE TABLE `user_achievement_streaks` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `aid` bigint NOT NULL default '0',
  `streak` bigint NOT NULL default '0',
  `last_hit` datetime NOT NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `achievement` (`uid`,`aid`)
);

--
-- Table structure for table `user_achievements`
--

DROP TABLE IF EXISTS `user_achievements`;
CREATE TABLE `user_achievements` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `aid` bigint NOT NULL default '0',
  `exponent` int NOT NULL default '0',
  `createtime` datetime NOT NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `achievement` (`uid`,`aid`),
  KEY `aid_exponent` (`aid`,`exponent`)
);

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `uid` bigint NOT NULL auto_increment,
  `nickname` varchar(20) NOT NULL default '',
  `realemail` varchar(50) NOT NULL default '',
  `fakeemail` varchar(50) default NULL,
  `homepage` varchar(100) default NULL,
  `passwd` varchar(32) NOT NULL default '',
  `sig` varchar(200) default NULL,
  `seclev` bigint NOT NULL default '0',
  `matchname` varchar(20) default NULL,
  `newpasswd` varchar(32) default '',
  `newpasswd_ts` datetime default NULL,
  `journal_last_entry_date` datetime default NULL,
  `author` int NOT NULL default '0',
  `shill_id` int NOT NULL default '0',
  PRIMARY KEY  (`uid`),
  KEY `chk4matchname` (`matchname`),
  KEY `author_lookup` (`author`),
  KEY `login` (`nickname`,`uid`,`passwd`),
  KEY `chk4user` (`realemail`,`nickname`),
  KEY `seclev` (`seclev`)
);

--
-- Table structure for table `users_acl`
--

DROP TABLE IF EXISTS `users_acl`;
CREATE TABLE `users_acl` (
  `id` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `acl` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `uid_key` (`uid`,`acl`),
  KEY `uid` (`uid`)
);

--
-- Table structure for table `users_clout`
--

DROP TABLE IF EXISTS `users_clout`;
CREATE TABLE `users_clout` (
  `clout_id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL,
  `clid` int NOT NULL,
  `clout` float default NULL,
  PRIMARY KEY  (`clout_id`),
  UNIQUE KEY `uid_clid` (`uid`,`clid`),
  KEY `clid` (`clid`)
);

--
-- Table structure for table `users_comments`
--

DROP TABLE IF EXISTS `users_comments`;
CREATE TABLE `users_comments` (
  `uid` bigint NOT NULL default '0',
  `points` int NOT NULL default '0',
  `posttype` bigint NOT NULL default '2',
  `defaultpoints` int NOT NULL default '1',
  `highlightthresh` int NOT NULL default '4',
  `maxcommentsize` int NOT NULL default '4096',
  `hardthresh` int NOT NULL default '0',
  `clbig` int NOT NULL default '0',
  `clsmall` int NOT NULL default '0',
  `reparent` int NOT NULL default '1',
  `nosigs` int NOT NULL default '0',
  `commentlimit` int NOT NULL default '100',
  `commentspill` int NOT NULL default '50',
  `commentsort` int NOT NULL default '0',
  `noscores` int NOT NULL default '0',
  `mode` enum('flat','nested','nocomment','thread') NOT NULL default 'thread',
  `threshold` int NOT NULL default '1',
  PRIMARY KEY  (`uid`),
  KEY `points` (`points`)
);

--
-- Table structure for table `users_comments_read_log`
--

DROP TABLE IF EXISTS `users_comments_read_log`;
CREATE TABLE `users_comments_read_log` (
  `uid` bigint NOT NULL,
  `discussion_id` bigint NOT NULL,
  `cid` int NOT NULL,
  UNIQUE KEY `discussion_id` (`discussion_id`,`uid`,`cid`)
);

--
-- Table structure for table `users_hits`
--

DROP TABLE IF EXISTS `users_hits`;
CREATE TABLE `users_hits` (
  `uid` bigint NOT NULL default '0',
  `lastclick` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `hits` int NOT NULL default '0',
  `hits_bought` int NOT NULL default '0',
  `hits_bought_today` int NOT NULL default '0',
  `hits_paidfor` int NOT NULL default '0',
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `users_index`
--

DROP TABLE IF EXISTS `users_index`;
CREATE TABLE `users_index` (
  `uid` bigint NOT NULL default '0',
  `story_never_topic` text NOT NULL,
  `story_never_author` varchar(255) NOT NULL default '',
  `story_never_nexus` varchar(255) NOT NULL default '',
  `slashboxes` text NOT NULL,
  `maxstories` int NOT NULL default '30',
  `noboxes` int NOT NULL default '0',
  `story_always_topic` text NOT NULL,
  `story_always_author` varchar(255) NOT NULL default '',
  `story_always_nexus` varchar(255) NOT NULL default '',
  `story_brief_best_nexus` varchar(255) NOT NULL default '',
  `story_full_brief_nexus` varchar(255) NOT NULL default '',
  `story_brief_always_nexus` varchar(255) NOT NULL default '',
  `story_full_best_nexus` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `users_info`
--

DROP TABLE IF EXISTS `users_info`;
CREATE TABLE `users_info` (
  `uid` bigint NOT NULL default '0',
  `totalmods` bigint NOT NULL default '0',
  `realname` varchar(50) default NULL,
  `bio` text NOT NULL,
  `tokens` bigint NOT NULL default '0',
  `lastgranted` datetime NOT NULL,
  `m2info` varchar(64) NOT NULL default '',
  `karma` bigint NOT NULL default '0',
  `maillist` int NOT NULL default '0',
  `totalcomments` bigint default '0',
  `lastm2` datetime NOT NULL default '1970-01-01 00:00:00',
  `m2_mods_saved` varchar(120) NOT NULL default '',
  `lastaccess` date NOT NULL,
  `m2fair` bigint NOT NULL default '0',
  `up_fair` bigint NOT NULL default '0',
  `down_fair` bigint NOT NULL default '0',
  `m2unfair` bigint NOT NULL default '0',
  `up_unfair` bigint NOT NULL default '0',
  `down_unfair` bigint NOT NULL default '0',
  `m2fairvotes` bigint NOT NULL default '0',
  `m2voted_up_fair` bigint NOT NULL default '0',
  `m2voted_down_fair` bigint NOT NULL default '0',
  `m2unfairvotes` bigint NOT NULL default '0',
  `m2voted_up_unfair` bigint NOT NULL default '0',
  `m2voted_down_unfair` bigint NOT NULL default '0',
  `m2voted_lonedissent` bigint NOT NULL default '0',
  `m2voted_majority` bigint NOT NULL default '0',
  `upmods` bigint NOT NULL default '0',
  `downmods` bigint NOT NULL default '0',
  `stirred` bigint NOT NULL default '0',
  `session_login` int NOT NULL default '0',
  `cookie_location` enum('classbid','subnetid','ipid','none') NOT NULL default 'none',
  `created_at` datetime NOT NULL,
  `tag_clout` float NOT NULL default '1',
  `registered` int NOT NULL default '1',
  `reg_id` varchar(32) NOT NULL default '',
  `expiry_days` int NOT NULL default '1',
  `expiry_comm` int NOT NULL default '1',
  `user_expiry_days` int NOT NULL default '1',
  `user_expiry_comm` int NOT NULL default '1',
  `initdomain` varchar(30) NOT NULL default '',
  `created_ipid` varchar(32) NOT NULL default '',
  `people` blob,
  `people_status` enum('ok','dirty') NOT NULL default 'ok',
  `csq_bonuses` float NOT NULL default '0',
  PRIMARY KEY  (`uid`),
  KEY `people_status` (`people_status`),
  KEY `initdomain` (`initdomain`),
  KEY `created_ipid` (`created_ipid`),
  KEY `tokens` (`tokens`)
);

--
-- Table structure for table `users_logtokens`
--

DROP TABLE IF EXISTS `users_logtokens`;
CREATE TABLE `users_logtokens` (
  `lid` bigint NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `locationid` varchar(32) NOT NULL default '',
  `temp` enum('yes','no') NOT NULL default 'no',
  `public` enum('yes','no') NOT NULL default 'no',
  `expires` datetime NOT NULL default '2000-01-01 00:00:00',
  `value` varchar(22) NOT NULL default '',
  PRIMARY KEY  (`lid`),
  UNIQUE KEY `uid_locationid_temp_public` (`uid`,`locationid`,`temp`,`public`),
  KEY `locationid` (`locationid`),
  KEY `temp` (`temp`),
  KEY `public` (`public`)
);

--
-- Table structure for table `users_messages`
--

DROP TABLE IF EXISTS `users_messages`;
CREATE TABLE `users_messages` (
  `id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL default '0',
  `code` int NOT NULL default '0',
  `mode` int NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `code_key` (`uid`,`code`)
);

--
-- Table structure for table `users_openid`
--

DROP TABLE IF EXISTS `users_openid`;
CREATE TABLE `users_openid` (
  `opid` int NOT NULL auto_increment,
  `openid_url` varchar(255) NOT NULL,
  `uid` bigint NOT NULL,
  PRIMARY KEY  (`opid`),
  UNIQUE KEY `openid_url` (`openid_url`),
  KEY `uid` (`uid`)
);

--
-- Table structure for table `users_openid_reskeys`
--

DROP TABLE IF EXISTS `users_openid_reskeys`;
CREATE TABLE `users_openid_reskeys` (
  `oprid` int NOT NULL auto_increment,
  `openid_url` varchar(255) NOT NULL,
  `reskey` varchar(20) NOT NULL default '',
  PRIMARY KEY  (`oprid`),
  KEY `openid_url` (`openid_url`),
  KEY `reskey` (`reskey`)
);

--
-- Table structure for table `users_param`
--

DROP TABLE IF EXISTS `users_param`;
CREATE TABLE `users_param` (
  `param_id` int NOT NULL auto_increment,
  `uid` bigint NOT NULL,
  `name` varchar(32) NOT NULL default '',
  `value` text NOT NULL,
  PRIMARY KEY  (`param_id`),
  UNIQUE KEY `uid_key` (`uid`,`name`),
  KEY `name` (`name`)
);

--
-- Table structure for table `users_prefs`
--

DROP TABLE IF EXISTS `users_prefs`;
CREATE TABLE `users_prefs` (
  `uid` bigint NOT NULL default '0',
  `willing` int NOT NULL default '1',
  `dfid` int NOT NULL default '0',
  `tzcode` varchar(4) NOT NULL default 'EST',
  `noicons` int NOT NULL default '0',
  `light` int NOT NULL default '0',
  `mylinks` varchar(255) NOT NULL default '',
  `lang` varchar(5) NOT NULL default 'en_US',
  PRIMARY KEY  (`uid`)
);

--
-- Table structure for table `vars`
--

DROP TABLE IF EXISTS `vars`;
CREATE TABLE `vars` (
  `name` varchar(48) NOT NULL default '',
  `value` text,
  `description` varchar(255) default NULL,
  PRIMARY KEY  (`name`)
);

--
-- Table structure for table `wow_char_armorylog`
--

DROP TABLE IF EXISTS `wow_char_armorylog`;
CREATE TABLE `wow_char_armorylog` (
  `arlid` int NOT NULL auto_increment,
  `charid` int NOT NULL,
  `ts` datetime NOT NULL,
  `armorydata` blob NOT NULL,
  `raw_content` blob,
  PRIMARY KEY  (`arlid`),
  KEY `ts` (`ts`),
  KEY `charid_ts` (`charid`,`ts`)
);

--
-- Table structure for table `wow_char_data`
--

DROP TABLE IF EXISTS `wow_char_data`;
CREATE TABLE `wow_char_data` (
  `wcdid` int NOT NULL auto_increment,
  `charid` int NOT NULL,
  `wcdtype` int NOT NULL,
  `value` varchar(100) default NULL,
  PRIMARY KEY  (`wcdid`),
  UNIQUE KEY `charid_wcdtype` (`charid`,`wcdtype`)
);

--
-- Table structure for table `wow_char_types`
--

DROP TABLE IF EXISTS `wow_char_types`;
CREATE TABLE `wow_char_types` (
  `wcdtype` int NOT NULL auto_increment,
  `name` varchar(100) NOT NULL,
  PRIMARY KEY  (`wcdtype`),
  UNIQUE KEY `name` (`name`)
);

--
-- Table structure for table `wow_chars`
--

DROP TABLE IF EXISTS `wow_chars`;
CREATE TABLE `wow_chars` (
  `charid` int NOT NULL auto_increment,
  `realmid` int NOT NULL,
  `charname` varchar(12) NOT NULL,
  `guildid` int default NULL,
  `uid` bigint default NULL,
  `last_retrieval_attempt` datetime default NULL,
  `last_retrieval_success` datetime default NULL,
  PRIMARY KEY  (`charid`),
  UNIQUE KEY `realm_name` (`realmid`,`charname`),
  KEY `name` (`charname`),
  KEY `uid` (`uid`),
  KEY `last_retrieval_success` (`last_retrieval_success`),
  KEY `last_retrieval_attempt` (`last_retrieval_attempt`)
);

--
-- Table structure for table `wow_guilds`
--

DROP TABLE IF EXISTS `wow_guilds`;
CREATE TABLE `wow_guilds` (
  `guildid` int NOT NULL auto_increment,
  `realmid` int NOT NULL,
  `guildname` varchar(64) NOT NULL,
  PRIMARY KEY  (`guildid`),
  UNIQUE KEY `idx_name` (`realmid`,`guildname`)
);

--
-- Table structure for table `wow_realms`
--

DROP TABLE IF EXISTS `wow_realms`;
CREATE TABLE `wow_realms` (
  `realmid` int NOT NULL auto_increment,
  `countryname` varchar(2) NOT NULL,
  `realmname` varchar(64) NOT NULL,
  `type` enum('pve','pvp','rp','rppvp') NOT NULL default 'pve',
  `battlegroup` varchar(16) default NULL,
  PRIMARY KEY  (`realmid`),
  UNIQUE KEY `country_realm` (`countryname`,`realmname`),
  KEY `battlegroup` (`countryname`,`battlegroup`)
);

--
-- Table structure for table `xsite_auth_log`
--

DROP TABLE IF EXISTS `xsite_auth_log`;
CREATE TABLE `xsite_auth_log` (
  `site` varchar(30) NOT NULL default '',
  `ts` datetime NOT NULL,
  `nonce` varchar(30) NOT NULL default '',
  UNIQUE KEY `site` (`site`,`ts`,`nonce`)
);
