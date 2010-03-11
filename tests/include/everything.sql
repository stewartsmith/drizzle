-- 
--  Schema for "Everything"
--

--
-- Table structure for table `branch`
--

DROP TABLE IF EXISTS `branch`;
CREATE TABLE `branch` (
  `branch_id` int NOT NULL default '0',
  `project_id` int NOT NULL default '0',
  PRIMARY KEY  (`branch_id`)
) ;

--
-- Table structure for table `container`
--

DROP TABLE IF EXISTS `container`;
CREATE TABLE `container` (
  `container_id` int NOT NULL auto_increment,
  `context` text,
  `parent_container` int default NULL,
  PRIMARY KEY  (`container_id`)
)  AUTO_INCREMENT=312;

--
-- Table structure for table `document`
--

DROP TABLE IF EXISTS `document`;
CREATE TABLE `document` (
  `document_id` int NOT NULL auto_increment,
  `doctext` text,
  PRIMARY KEY  (`document_id`)
)  AUTO_INCREMENT=623;

--
-- Table structure for table `genstats_attributes`
--

DROP TABLE IF EXISTS `genstats_attributes`;
CREATE TABLE `genstats_attributes` (
  `genstats_attributes_id` int NOT NULL default '0',
  `predicates` text NOT NULL,
  `attributes` text NOT NULL,
  PRIMARY KEY  (`genstats_attributes_id`)
) ENGINE=InnoDB;

--
-- Table structure for table `htmlcode`
--

DROP TABLE IF EXISTS `htmlcode`;
CREATE TABLE `htmlcode` (
  `htmlcode_id` int NOT NULL auto_increment,
  `code` text,
  PRIMARY KEY  (`htmlcode_id`)
)  AUTO_INCREMENT=516;

--
-- Table structure for table `htmlpage`
--

DROP TABLE IF EXISTS `htmlpage`;
CREATE TABLE `htmlpage` (
  `htmlpage_id` int NOT NULL auto_increment,
  `pagetype_nodetype` int default NULL,
  `displaytype` varchar(20) default NULL,
  `page` text,
  `parent_container` int default NULL,
  `ownedby_theme` int NOT NULL default '0',
  `permissionneeded` char(1) NOT NULL default 'r',
  `MIMEtype` varchar(255) NOT NULL default 'text/html',
  PRIMARY KEY  (`htmlpage_id`)
)  AUTO_INCREMENT=564;

--
-- Table structure for table `image`
--

DROP TABLE IF EXISTS `image`;
CREATE TABLE `image` (
  `image_id` int NOT NULL auto_increment,
  `src` varchar(255) default NULL,
  `alt` varchar(255) default NULL,
  `thumbsrc` varchar(255) default NULL,
  `description` text,
  PRIMARY KEY  (`image_id`)
)  AUTO_INCREMENT=138;

--
-- Table structure for table `javascript`
--

DROP TABLE IF EXISTS `javascript`;
CREATE TABLE `javascript` (
  `javascript_id` int NOT NULL default '0',
  `code` text NOT NULL,
  `comment` text NOT NULL,
  `dynamic` int NOT NULL default '0',
  PRIMARY KEY  (`javascript_id`)
) ;

--
-- Table structure for table `knowledge_item`
--

DROP TABLE IF EXISTS `knowledge_item`;
CREATE TABLE `knowledge_item` (
  `knowledge_item_id` int NOT NULL default '0',
  `item` text NOT NULL,
  `question` int NOT NULL default '0',
  PRIMARY KEY  (`knowledge_item_id`)
) ;

--
-- Table structure for table `links`
--

DROP TABLE IF EXISTS `links`;
CREATE TABLE `links` (
  `from_node` int NOT NULL default '0',
  `to_node` int NOT NULL default '0',
  `linktype` int NOT NULL default '0',
  `hits` int default '0',
  `food` int default '0',
  PRIMARY KEY  (`from_node`,`to_node`,`linktype`)
) ;

--
-- Table structure for table `mail`
--

DROP TABLE IF EXISTS `mail`;
CREATE TABLE `mail` (
  `mail_id` int NOT NULL default '0',
  `from_address` char(80) NOT NULL default '',
  `attachment_file` int NOT NULL default '0',
  PRIMARY KEY  (`mail_id`)
) ;

--
-- Table structure for table `node`
--

DROP TABLE IF EXISTS `node`;
CREATE TABLE `node` (
  `node_id` int NOT NULL auto_increment,
  `type_nodetype` int NOT NULL default '0',
  `title` char(240) NOT NULL default '',
  `author_user` int NOT NULL default '0',
  `createtime` datetime NOT NULL,
  `modified` datetime NOT NULL,
  `hits` int default '0',
  `loc_location` int default '0',
  `reputation` int NOT NULL default '0',
  `lockedby_user` int NOT NULL default '0',
  `locktime` datetime NOT NULL,
  `authoraccess` char(4) NOT NULL default 'iiii',
  `groupaccess` char(5) NOT NULL default 'iiiii',
  `otheraccess` char(5) NOT NULL default 'iiiii',
  `guestaccess` char(5) NOT NULL default 'iiiii',
  `dynamicauthor_permission` int NOT NULL default '-1',
  `dynamicgroup_permission` int NOT NULL default '-1',
  `dynamicother_permission` int NOT NULL default '-1',
  `dynamicguest_permission` int NOT NULL default '-1',
  `group_usergroup` int NOT NULL default '-1',
  PRIMARY KEY  (`node_id`),
  KEY `title` (`title`,`type_nodetype`),
  KEY `author` (`author_user`),
  KEY `type` (`type_nodetype`)
)  AUTO_INCREMENT=641;

--
-- Table structure for table `nodegroup`
--

DROP TABLE IF EXISTS `nodegroup`;
CREATE TABLE `nodegroup` (
  `nodegroup_id` int NOT NULL auto_increment,
  `rank` int NOT NULL default '0',
  `node_id` int NOT NULL default '0',
  `orderby` int default NULL,
  PRIMARY KEY  (`nodegroup_id`,`rank`)
)  AUTO_INCREMENT=624;

--
-- Table structure for table `nodelet`
--

DROP TABLE IF EXISTS `nodelet`;
CREATE TABLE `nodelet` (
  `nodelet_id` int NOT NULL auto_increment,
  `nltext` text,
  `nlcode` text,
  `nlgoto` int default NULL,
  `parent_container` int default NULL,
  `lastupdate` int NOT NULL default '0',
  `updateinterval` int NOT NULL default '0',
  `mini_nodelet` int NOT NULL default '0',
  PRIMARY KEY  (`nodelet_id`)
)  AUTO_INCREMENT=495;

--
-- Table structure for table `nodemethod`
--

DROP TABLE IF EXISTS `nodemethod`;
CREATE TABLE `nodemethod` (
  `nodemethod_id` int NOT NULL default '0',
  `supports_nodetype` int NOT NULL default '0',
  `code` text NOT NULL,
  PRIMARY KEY  (`nodemethod_id`)
) ;

--
-- Table structure for table `nodetype`
--

DROP TABLE IF EXISTS `nodetype`;
CREATE TABLE `nodetype` (
  `nodetype_id` int NOT NULL auto_increment,
  `restrict_nodetype` int default '0',
  `extends_nodetype` int default '0',
  `restrictdupes` int default '0',
  `sqltable` char(255) default NULL,
  `grouptable` char(40) default '',
  `defaultauthoraccess` char(4) NOT NULL default 'iiii',
  `defaultgroupaccess` char(5) NOT NULL default 'iiiii',
  `defaultotheraccess` char(5) NOT NULL default 'iiiii',
  `defaultguestaccess` char(5) NOT NULL default 'iiiii',
  `defaultgroup_usergroup` int NOT NULL default '-1',
  `defaultauthor_permission` int NOT NULL default '-1',
  `defaultgroup_permission` int NOT NULL default '-1',
  `defaultother_permission` int NOT NULL default '-1',
  `defaultguest_permission` int NOT NULL default '-1',
  `maxrevisions` int NOT NULL default '-1',
  `canworkspace` int NOT NULL default '-1',
  PRIMARY KEY  (`nodetype_id`)
)  AUTO_INCREMENT=561;

--
-- Table structure for table `project`
--

DROP TABLE IF EXISTS `project`;
CREATE TABLE `project` (
  `project_id` int NOT NULL default '0',
  `description` text NOT NULL,
  `last_update` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `short_description` varchar(250) NOT NULL default '',
  `url_cvs` varchar(250) NOT NULL default '',
  `url_faq` varchar(250) NOT NULL default '',
  `long_description` text NOT NULL,
  PRIMARY KEY  (`project_id`)
) ;

--
-- Table structure for table `project_document`
--

DROP TABLE IF EXISTS `project_document`;
CREATE TABLE `project_document` (
  `project_document_id` int NOT NULL default '0',
  `project_id` int NOT NULL default '0',
  PRIMARY KEY  (`project_document_id`)
) ;

--
-- Table structure for table `question`
--

DROP TABLE IF EXISTS `question`;
CREATE TABLE `question` (
  `question_id` int NOT NULL default '0',
  `faq_id` int NOT NULL default '0',
  `project_id` int NOT NULL default '0',
  `rank` int NOT NULL default '0',
  `orderby` int NOT NULL default '0',
  `description` text NOT NULL,
  PRIMARY KEY  (`question_id`,`rank`)
) ;

--
-- Table structure for table `redirects`
--

DROP TABLE IF EXISTS `redirects`;
CREATE TABLE `redirects` (
  `redirects_id` int NOT NULL default '0',
  `url` text NOT NULL,
  PRIMARY KEY  (`redirects_id`)
) ;

--
-- Table structure for table `releases`
--

DROP TABLE IF EXISTS `releases`;
CREATE TABLE `releases` (
  `releases_id` int NOT NULL default '0',
  `branch_id` int NOT NULL default '0',
  `description` text,
  `url_targz` varchar(250) NOT NULL default '',
  `url_osx` varchar(250) NOT NULL default '',
  `url_rpm` varchar(250) NOT NULL default '',
  `project_id` int NOT NULL default '0',
  `version` varchar(30) NOT NULL default 'latest',
  `created` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `url_source_rpm` char(250) NOT NULL,
  PRIMARY KEY  (`releases_id`)
) ;

--
-- Table structure for table `revision`
--

DROP TABLE IF EXISTS `revision`;
CREATE TABLE `revision` (
  `node_id` int NOT NULL default '0',
  `inside_workspace` int NOT NULL default '0',
  `revision_id` int NOT NULL default '0',
  `xml` text NOT NULL,
  `tstamp` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`node_id`,`inside_workspace`,`revision_id`)
) ;

--
-- Table structure for table `setting`
--

DROP TABLE IF EXISTS `setting`;
CREATE TABLE `setting` (
  `setting_id` int NOT NULL auto_increment,
  `vars` text NOT NULL,
  PRIMARY KEY  (`setting_id`)
)  AUTO_INCREMENT=623;

--
-- Table structure for table `symlink`
--

DROP TABLE IF EXISTS `symlink`;
CREATE TABLE `symlink` (
  `symlink_id` int NOT NULL default '0',
  `symlink_node` int NOT NULL default '0',
  PRIMARY KEY  (`symlink_id`)
) ;

--
-- Table structure for table `themesetting`
--

DROP TABLE IF EXISTS `themesetting`;
CREATE TABLE `themesetting` (
  `themesetting_id` int NOT NULL default '0',
  `parent_theme` int NOT NULL default '0',
  PRIMARY KEY  (`themesetting_id`)
) ;

--
-- Table structure for table `typeversion`
--

DROP TABLE IF EXISTS `typeversion`;
CREATE TABLE `typeversion` (
  `typeversion_id` int NOT NULL default '0',
  `version` int NOT NULL default '0',
  PRIMARY KEY  (`typeversion_id`)
) ;

--
-- Table structure for table `user`
--

DROP TABLE IF EXISTS `user`;
CREATE TABLE `user` (
  `user_id` int NOT NULL auto_increment,
  `nick` varchar(20) default NULL,
  `passwd` varchar(10) default NULL,
  `realname` varchar(40) default NULL,
  `email` varchar(40) default NULL,
  `lasttime` datetime default NULL,
  `karma` int default '0',
  `inside_workspace` int NOT NULL default '0',
  PRIMARY KEY  (`user_id`)
)  AUTO_INCREMENT=623;

--
-- Table structure for table `version`
--

DROP TABLE IF EXISTS `version`;
CREATE TABLE `version` (
  `version_id` int NOT NULL default '0',
  `version` int NOT NULL default '1',
  PRIMARY KEY  (`version_id`)
) ;

--
-- Table structure for table `weblog`
--

DROP TABLE IF EXISTS `weblog`;
CREATE TABLE `weblog` (
  `tstamp` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `linkedby_user` int NOT NULL default '0',
  `removedby_user` int NOT NULL default '0',
  `linkedtime` datetime NOT NULL,
  `weblog_id` int NOT NULL default '0',
  `to_node` int NOT NULL default '0',
  `entry_id` int NOT NULL auto_increment,
  PRIMARY KEY  (`entry_id`),
  KEY `tstamp` (`tstamp`)
) ;

--
-- Table structure for table `workspace`
--

DROP TABLE IF EXISTS `workspace`;
CREATE TABLE `workspace` (
  `workspace_id` int NOT NULL default '0',
  PRIMARY KEY  (`workspace_id`)
) ;
