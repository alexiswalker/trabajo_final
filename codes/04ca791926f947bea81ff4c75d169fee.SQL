column sid      format 999999   ;
column serial#  format 999999   ;
column sql_id   format a13      ;
column x        format a1       ;
column status   format a10      ;
column program  format a20      ;
column sql      format a80      ;
column event    format a40      ;
column ospid    format a9       ;
column obj_name format a40      ;
select
   decode(ss.sid,sys_context('USERENV','SID'),'X') X
  ,ss.inst_id
  ,ss.status
  ,ss.sid
  ,ss.serial#
  ,p.spid as ospid
  ,ss.program
  ,event
  ,sql_id
  ,(select substr(sql_text,1,60) from v$sql s where s.sql_id=ss.sql_id and rownum=1) sql
  ,ss.row_wait_obj#
  ,(select nvl(subobject_name,object_name) from dba_objects o where object_id=row_wait_obj#) obj_name
from gv$session ss
    ,gv$process p
where 
      ss.osuser   = sys_context('USERENV','OS_USER')
  and ss.terminal = userenv('terminal') 
  and ss.paddr   = p.addr
  and ss.inst_id = p.inst_id
order by ss.status;

column sid      clear;
column serial#  clear;
column x        clear;
column status   clear;
column program  clear;
column sql      clear;
column event    clear;
column ospid    clear;
column obj_name clear;