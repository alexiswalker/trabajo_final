@inc/input_vars_init
prompt Show only first 30 found:

col if_all new_val _if_all noprint;

select case when q'[&2 &3 &4 &5]' like '%all%' then '--'
            else '  '
       end if_all
from dual;

col sql_id          format a13;
col signature       format a21;
col sql_text_trunc  format a300 word;
col to_purge        format a30;

SELECT/*+NOTME*/ *
from ( select
              inst_id
            , sa.sql_id
            , to_char(sa.force_matching_signature,'tm9')                   as signature
            , sa.ADDRESS || ',' || sa.HASH_VALUE                           as to_purge
            , sa.plan_hash_value                                           as phv
            , sa.executions                                                as execs
            , sa.elapsed_time/1e6/decode(sa.executions,0,1,sa.executions)  as elaexe
            , regexp_replace(substr(sql_text,1,300),'[^[:ALNUM] ]*',' ')        as sql_text_trunc
            , sa.sql_profile
       FROM gv$sql sa
       where
                     upper(sa.sql_text) like upper(q'[&1]')
                 and sql_text not like 'SELECT/*+NOTME*/%'
       &_if_all  and sa.COMMAND_TYPE=3
       order by elapsed_time desc,executions desc
)
where
   rownum<=30
/
undef _if_all;
col if_all              clear;
col sql_id              clear;
col sql_text_trunc      clear;
col to_purge            clear;
@inc/input_vars_undef;