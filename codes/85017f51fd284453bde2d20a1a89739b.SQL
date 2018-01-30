@inc/input_vars_init;
prompt *** Show top session by undo used:
prompt;
col username         format a25;
col osuser           format a25;
col process          format a13;
col object           format a40;
col subobject_name   format a30;
col object_type      format a20;
with t as (
   select *
   from (select *
         from v$transaction
         where used_ublk>0
         order by used_ublk desc)
   where rownum<=10
)
   select
      s.sid
     ,s.serial#
     ,s.username
     ,s.osuser
     ,t.used_urec   as recs
     ,t.used_ublk   as blocks
     ,o.owner||'.'||o.object_name object
     ,o.subobject_name
     ,o.data_object_id
     ,o.object_type
     ,l.object_id
     ,l.xidusn
     ,l.xidslot
     ,l.xidsqn
     ,l.process
   from t
      , v$session s
      , v$locked_object l
      , dba_objects o
    where t.addr      = s.taddr
      and t.xidusn    = l.xidusn(+)
      and t.xidslot   = l.xidslot(+)
      and l.object_id = o.object_id(+)
   order by blocks desc,sid
/
col username         clear;
col osuser           clear;
col process          clear;
col object           clear;
col subobject_name   clear;
col object_type      clear;
@inc/input_vars_undef;