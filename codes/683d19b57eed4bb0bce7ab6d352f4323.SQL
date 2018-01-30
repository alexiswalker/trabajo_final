@inc/input_vars_init;
col username      format a20  heading USER;
col inst_id       format 9999;
col osuser        format a18;
col process       format a12;
col program       format a20;
col module        format a20;
col terminal      format a20;
col type          format a10;
col sql_id        format a13;
col action        format a17;
col event         format a30;
col wait_class    format a20;
col sql_exec_start format a19;
col objname       format a30;
select 
     s.sid,s.serial#
    ,s.inst_id
    ,s.username
    --,s.schemaname
    ,s.osuser,s.process
    ,substr(s.program,1,20) program
    ,s.module
    ,s.terminal,s.type
&_IF_ORA11_OR_HIGHER    ,to_char(s.SQL_EXEC_START,'dd/mm hh24:mi:ss') sql_exec_start
    ,s.sql_id
    ,s.action
    ,s.event
    ,s.wait_class
    --,s.row_wait_obj#
    ,(select object_name from dba_objects o where object_id=s.row_wait_obj#) objname
from gv$session s 
where 
     s.sid!=sys_context('userenv','sid')
    and (
              upper(s.wait_class) like upper('%'||'&1'||'%') 
           or upper(s.event)      like upper('%'||'&1'||'%')
    )
order by s.type,s.osuser
/
col username      clear;
col inst_id       clear;
col osuser        clear;
col process       clear;
col module        clear;
col program       clear;
col terminal      clear;
col type          clear;
--col sql_id        clear;
col action        clear;
col event         clear;
col wait_class    clear;
col sql_exec_start clear;
col objname       clear;
@inc/input_vars_undef;