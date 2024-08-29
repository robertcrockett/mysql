/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/table_esgs_by_user_by_event_name.cc
  Table EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_esgs_by_user_by_event_name.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_esgs_by_user_by_event_name::m_table_lock;

Plugin_table table_esgs_by_user_by_event_name::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_stages_summary_by_user_by_event_name",
    /* Definition */
    "  USER CHAR(32) collate utf8mb4_bin default null,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  UNIQUE KEY (USER, EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_esgs_by_user_by_event_name::m_share = {
    &pfs_truncatable_acl,
    table_esgs_by_user_by_event_name::create,
    nullptr, /* write_row */
    table_esgs_by_user_by_event_name::delete_all_rows,
    table_esgs_by_user_by_event_name::get_row_count,
    sizeof(pos_esgs_by_user_by_event_name),
    &m_table_lock,
    &m_table_def,
    false /* perpetual */,
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_esgs_by_user_by_event_name::match(PFS_user *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_esgs_by_user_by_event_name::match(PFS_instr_class *instr_class) {
  if (m_fields >= 2) {
    if (!m_key_2.match(instr_class)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_esgs_by_user_by_event_name::create(
    PFS_engine_table_share *) {
  return new table_esgs_by_user_by_event_name();
}

int table_esgs_by_user_by_event_name::delete_all_rows() {
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  reset_events_stages_by_user();
  return 0;
}

ha_rows table_esgs_by_user_by_event_name::get_row_count() {
  return global_user_container.get_row_count() * stage_class_max;
}

table_esgs_by_user_by_event_name::table_esgs_by_user_by_event_name()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {
  m_normalizer = time_normalizer::get_stage();
}

void table_esgs_by_user_by_event_name::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_esgs_by_user_by_event_name::rnd_init(bool) { return 0; }

int table_esgs_by_user_by_event_name::rnd_next() {
  PFS_user *user;
  PFS_stage_class *stage_class;
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user()) {
    user = global_user_container.get(m_pos.m_index_1, &has_more_user);
    if (user != nullptr) {
      stage_class = find_stage_class(m_pos.m_index_2);
      if (stage_class) {
        m_next_pos.set_after(&m_pos);
        return make_row(user, stage_class);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esgs_by_user_by_event_name::rnd_pos(const void *pos) {
  PFS_user *user;
  PFS_stage_class *stage_class;

  set_position(pos);

  user = global_user_container.get(m_pos.m_index_1);
  if (user != nullptr) {
    stage_class = find_stage_class(m_pos.m_index_2);
    if (stage_class) {
      return make_row(user, stage_class);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_esgs_by_user_by_event_name::index_init(uint idx [[maybe_unused]],
                                                 bool) {
  PFS_index_esgs_by_user_by_event_name *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_esgs_by_user_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_esgs_by_user_by_event_name::index_next() {
  PFS_user *user;
  PFS_stage_class *stage_class;
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user()) {
    user = global_user_container.get(m_pos.m_index_1, &has_more_user);
    if (user != nullptr) {
      if (m_opened_index->match(user)) {
        do {
          stage_class = find_stage_class(m_pos.m_index_2);
          if (stage_class) {
            if (m_opened_index->match(stage_class)) {
              if (!make_row(user, stage_class)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.m_index_2++;
          }
        } while (stage_class != nullptr);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_esgs_by_user_by_event_name::make_row(PFS_user *user,
                                               PFS_stage_class *klass) {
  pfs_optimistic_state lock;

  user->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_user.make_row(user)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_event_name.make_row(klass);

  PFS_connection_stage_visitor visitor(klass);
  PFS_connection_iterator::visit_user(user, true, /* accounts */
                                      true,       /* threads */
                                      false,      /* THDs */
                                      &visitor);

  if (!user->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);

  return 0;
}

int table_esgs_by_user_by_event_name::read_row_values(TABLE *table,
                                                      unsigned char *buf,
                                                      Field **fields,
                                                      bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* USER */
          m_row.m_user.set_nullable_field(f);
          break;
        case 1: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        default: /* 2, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stat.set_field(f->field_index() - 2, f);
          break;
      }
    }
  }

  return 0;
}
