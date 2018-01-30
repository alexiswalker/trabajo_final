@inc/input_vars_init;
prompt &_C_REVERSE *** Show text from dba_source by object name: &_C_RESET
prompt *** Usage: @src object_mask [owner_mask [line_start] [line_end]]
col owner format a30;
col text  format a80;
set serverout on;
declare 
   prev_obj varchar2(92);
begin
   for r in (  select src.owner,src.name,src.type,src.line,rtrim(src.text,chr(10)) text
                     ,max(length(owner))over() len_owner
                     ,max(length(name))over()  len_name
                     ,max(length(type))over()  len_type
                     ,max(length(text))over()  len_text
               from dba_source src
               where src.owner like nvl(upper('&2'),'%')
                 and src.name like upper('&1')
                 and src.line >= to_number(nvl('&3',1))
                 and src.line <= to_number(nvl('&4',1e5))
               order by 1,2,3,4
            )
   loop
      dbms_output.put_line(
                            rpad(r.owner,r.len_owner) 
                || ' | ' || rpad(r.name ,r.len_name )
                || ' | ' || rpad(r.type ,r.len_type )
                || ' | ' || to_char(r.line,'9999')
                || ' | ' || rpad(r.text ,r.len_text )
      );
      prev_obj:=r.owner||'.'||r.name||'.'||r.type;
   end loop;
end;
/
set serverout off;
col text clear;
@inc/input_vars_undef;
