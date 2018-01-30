col username            for a30;
col password            noprint;
col account_status      for a30;
col external_name       for a30;
col password_version    noprint;
col default_ts          for a30;
col temp_ts             for a30;
col profile             for a30;
col rsrc_group          for a30;
select
     USER_ID
    ,USERNAME
    ,PASSWORD
    ,ACCOUNT_STATUS
    ,CREATED
    ,LOCK_DATE
    ,EXPIRY_DATE
    ,DEFAULT_TABLESPACE     as default_ts
    ,TEMPORARY_TABLESPACE   as temp_ts
    ,PROFILE
    ,INITIAL_RSRC_CONSUMER_GROUP as rsrc_group
    ,EXTERNAL_NAME
&_IF_ORA11_OR_HIGHER    ,PASSWORD_VERSIONS
&_IF_ORA11_OR_HIGHER    ,EDITIONS_ENABLED
&_IF_ORA11_OR_HIGHER    ,AUTHENTICATION_TYPE
from dba_users u
where 
upper(u.username) like upper('&1')
order by user_id
/
col username            clear;
col password            clear;
col account_status      clear;
col external_name       clear;
col password_version    clear;
col default_ts          clear;
col temp_ts             clear;
col profile             clear;
col rsrc_group          clear;